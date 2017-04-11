#include "GeometryData.h"
#include <iostream>
#include "TriangleLUT.h"

GeometryData::GeometryData(unsigned width, unsigned height, unsigned depth, TerrainType::Enum type, ID3D11Device* device, ID3D11DeviceContext* deviceContext)
	: m_width(width), m_height(height), m_depth(depth)
{

	m_cubeSize = DirectX::XMFLOAT3(32.0f, 32.0f, 32.0f);
	//2.0f to decrease density
	m_cubeStep = DirectX::XMFLOAT3(2.0f / m_cubeSize.x, 2.0f / m_cubeSize.y, 2.0f / m_cubeSize.z);
	worldMatrix = DirectX::XMMatrixIdentity();

	switch(type)
	{
	case TerrainType::CUBE:
		GenerateCubeData();
		break;
	}

	m_texDesc = CreateTextureDesc();
	m_subData = CreateSubresourceData();
	m_texture3D = CreateTexture(device, m_texDesc, m_subData);
	m_densityMap = CreateDensityShaderResource(device, m_texture3D);
	m_triangleLUT = CreateTriangleLUTShaderResource(device);
	m_sampler = CreateSamplerState(device);
	InitializeBuffers(device);
	GenerateDecalDescriptionBuffer(device, deviceContext);
}

GeometryData::~GeometryData()
{
	if(m_vertexBuffer)
	{
		m_vertexBuffer->Release();
		m_vertexBuffer = nullptr;
	}

	if (m_densityMap)
	{
		m_densityMap->Release();
		m_densityMap = nullptr;
	}

	if (m_texture3D)
	{
		m_texture3D->Release();
		m_texture3D = nullptr;
	}

	delete m_data;
}

void GeometryData::GenerateCubeData()
{
	m_data = new float[m_width*m_depth*m_height];

	unsigned int width_offset = m_width / 4;
	unsigned int height_offset = m_height / 4;
	unsigned int depth_offset = m_depth / 4;

	size_t index = 0;

	for(size_t i = 0u; i < m_depth; ++i)
	{
		for (size_t j = 0u; j < m_height; ++j)
		{
			for (size_t k = 0u; k < m_width; ++k)
			{

				if(
					k >= 0 + width_offset && k <= m_width - width_offset 
					&& j >= 0 + height_offset && j <= m_height - height_offset
					&& i >= 0 + depth_offset && i <= m_depth - depth_offset)
				{
					m_data[index] = 1.0f;
				} else
				{
					m_data[index] = -1.0f;
				}

				index++;

			}
		}
	}


}

int GeometryData::GetVertices(VertexInputType** outVertices)
{
	int size = int(2.0f / m_cubeStep.x);
	size = size * size * size;
	m_vertexCount = size;

	(*outVertices) = new VertexInputType[size];
	int idx = 0;
	for (float z = -1; z < 1.0f; z += m_cubeStep.z)
	{
		for (float y = -1; y < 1.0f; y += m_cubeStep.y)
		{
			for (float x = -1; x < 1.0f; x += m_cubeStep.x)
			{
				(*outVertices)[idx].position = DirectX::XMFLOAT3(x, y, z);
				(*outVertices)[idx].color = DirectX::XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f);

				idx++;
			}
		}
	}

	return size;
}


void GeometryData::InitializeBuffers(ID3D11Device* device)
{
	VertexInputType* vertices = nullptr;
	D3D11_BUFFER_DESC vertexBufferDesc;
	D3D11_SUBRESOURCE_DATA vertexData;


	// Set the number of vertices in the vertex array.
	m_vertexCount = GetVertices(&vertices);

	// Set up the description of the static vertex buffer.
	vertexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	vertexBufferDesc.ByteWidth = sizeof(VertexInputType) * m_vertexCount;
	vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vertexBufferDesc.CPUAccessFlags = 0;
	vertexBufferDesc.MiscFlags = 0;
	vertexBufferDesc.StructureByteStride = 0;

	// Give the subresource structure a pointer to the vertex data.
	vertexData.pSysMem = vertices;
	vertexData.SysMemPitch = 0;
	vertexData.SysMemSlicePitch = 0;

	// Now create the vertex buffer.
	device->CreateBuffer(&vertexBufferDesc, &vertexData, &m_vertexBuffer);

	// Release the arrays now that the vertex and index buffers have been created and loaded.
	delete[] vertices;
	vertices = nullptr;

}

D3D11_TEXTURE3D_DESC GeometryData::CreateTextureDesc() const
{
	D3D11_TEXTURE3D_DESC output;

	output.Width = m_width;
	output.Height = m_height;
	output.Depth = m_depth;
	output.MipLevels = 1;
	output.Format = DXGI_FORMAT_R32_FLOAT;
	output.Usage = D3D11_USAGE_DEFAULT;
	output.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	output.CPUAccessFlags = 0;
	output.MiscFlags = 0;

	return output;
}

D3D11_SUBRESOURCE_DATA GeometryData::CreateSubresourceData() const
{
	D3D11_SUBRESOURCE_DATA output;

	ZeroMemory(&output, sizeof(output));

	//Size of a 1d line. Eg. distance between adjacent values
	output.SysMemPitch = m_width * sizeof(float);

	//Size of a 2D slice
	output.SysMemSlicePitch = m_width * m_height * sizeof(float);

	//The actual data
	output.pSysMem = m_data;

	return output;
}

ID3D11Texture3D* GeometryData::CreateTexture(ID3D11Device* device, const D3D11_TEXTURE3D_DESC texDesc, const D3D11_SUBRESOURCE_DATA subData) const
{
	ID3D11Texture3D* output;

	device->CreateTexture3D(&texDesc, &subData, &output);

	return output;
}

ID3D11ShaderResourceView* GeometryData::CreateDensityShaderResource(ID3D11Device* device, ID3D11Texture3D* texture3D) const
{
	ID3D11ShaderResourceView* output;

	device->CreateShaderResourceView(texture3D, nullptr, &output);

	return output;
}

ID3D11ShaderResourceView* GeometryData::CreateTriangleLUTShaderResource(ID3D11Device* device) const 
{
	ID3D11ShaderResourceView* output;

	D3D11_TEXTURE2D_DESC desc;
	ZeroMemory(&desc, sizeof(desc));
	desc.Height = 256;
	desc.Width = 16;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.Format = DXGI_FORMAT_R32_SINT;
	desc.SampleDesc = { 1, 0 };
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	desc.CPUAccessFlags = 0;
	desc.MiscFlags = 0;

	D3D11_SUBRESOURCE_DATA initData;
	ZeroMemory(&initData, sizeof(initData));
	initData.SysMemPitch = 16 * sizeof(int);
	initData.SysMemSlicePitch = 0;

	initData.pSysMem = TriangleLUT::TriTable;

	ID3D11Texture2D* texture = nullptr;
	device->CreateTexture2D(&desc, &initData, &texture);
	device->CreateShaderResourceView(texture, nullptr, &output);

	return output;
}

ID3D11SamplerState* GeometryData::CreateSamplerState(ID3D11Device* device) const
{
	ID3D11SamplerState* output;

	//Create a basic point sampler for sampling our density data in the gpu
	//should refactor this elsewhere
	D3D11_SAMPLER_DESC sampDesc;
	ZeroMemory(&sampDesc, sizeof(sampDesc));
	sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
	sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
	sampDesc.MinLOD = 0;
	sampDesc.MaxLOD = 0;

	device->CreateSamplerState(&sampDesc, &output);

	return output;
}

void GeometryData::GenerateDecalDescriptionBuffer(ID3D11Device* device, ID3D11DeviceContext* deviceContext)
{
	D3D11_BUFFER_DESC bd;
	ZeroMemory(&bd, sizeof(bd));
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.ByteWidth = sizeof(DecalDescription);
	bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	bd.CPUAccessFlags = 0;
	device->CreateBuffer(&bd, nullptr, &m_decalDescriptionBuffer);
	//return S_OK;

	DecalDescription dbuffer;
	dbuffer = GetDecals();
	deviceContext->UpdateSubresource(m_decalDescriptionBuffer, 0, nullptr, &dbuffer, 0, 0);
}

GeometryData::DecalDescription GeometryData::GetDecals() const
{
	DecalDescription buffer;

	ZeroMemory(&buffer, sizeof(buffer));
	buffer.decal[0] = DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 1);
	buffer.decal[1] = DirectX::XMFLOAT4(m_cubeStep.x, 0.0f, 0.0f, 1);
	buffer.decal[2] = DirectX::XMFLOAT4(m_cubeStep.x, m_cubeStep.y, 0.0f, 1);
	buffer.decal[3] = DirectX::XMFLOAT4(0.0f, m_cubeStep.y, 0.0f, 1);
	buffer.decal[4] = DirectX::XMFLOAT4(0.0f, 0.0f, m_cubeStep.z, 1);
	buffer.decal[5] = DirectX::XMFLOAT4(m_cubeStep.x, 0.0f, m_cubeStep.z, 1);
	buffer.decal[6] = DirectX::XMFLOAT4(m_cubeStep.x, m_cubeStep.y, m_cubeStep.z, 1);
	buffer.decal[7] = DirectX::XMFLOAT4(0.0f, m_cubeStep.y, m_cubeStep.z, 1);

	buffer.dataStep = DirectX::XMFLOAT4(1.0f / static_cast<float>(m_width), 1.0f / static_cast<float>(m_height), 1.0f / static_cast<float>(m_depth), 1);

	return buffer;
}

void GeometryData::DebugPrint()
{
	char* output = new char[m_width + 1];
	size_t index = 0;

	for (size_t i = 0u; i < m_depth; ++i)
	{
		for (size_t j = 0u; j < m_height; ++j)
		{
			for (size_t k = 0u; k < m_width; ++k)
			{
				if(m_data[index] == -1)
				{
					output[k] = '0';
				} else
				{
					output[k] = '1';
				}

				index++;
			}
			output[m_width] = '\0';
			printf("%s\n", output);
		}

		printf("\n===========================\n\n");
	}
}

void GeometryData::Render(ID3D11DeviceContext* deviceContext)
{
	unsigned int stride;
	unsigned int offset;

	// Set vertex buffer stride and offset.
	stride = sizeof(VertexInputType);
	offset = 0;

	// Set the vertex buffer to active in the input assembler so it can be rendered.
	deviceContext->IASetVertexBuffers(0, 1, &m_vertexBuffer, &stride, &offset);

	// Set the type of primitive that should be rendered from this vertex buffer, in this case triangles.
	deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);

	//Density Map to use
	deviceContext->GSSetShaderResources(0, 1, &m_densityMap);
	deviceContext->GSSetShaderResources(1, 1, &m_triangleLUT);
	//Set point sampler to use in the geometry shader
	deviceContext->GSSetSamplers(0, 1, &m_sampler);
	deviceContext->GSSetConstantBuffers(1, 1, &m_decalDescriptionBuffer);
}

unsigned GeometryData::GetVertexCount()
{
	return m_vertexCount;
}
