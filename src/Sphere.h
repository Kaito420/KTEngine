//=====================================================================================
// Sphere.h
// Author:Kaito Aoki
// Date:2025/09/05
//=====================================================================================

#ifndef _SPHERE_H_
#define _SPHERE_H_

#include "Component.h"
#include "RendererDX11.h"
#include <vector>

class Sphere : public Component {
private:
	ComPtr<ID3D11Buffer> _vertexBuffer;
	ComPtr<ID3D11Buffer> _indexBuffer;
	int _indexCount = 0;

	void CreateSphereMesh(float radius, int sliceCount, int stackCount,
		std::vector<Vertex>& vertices, std::vector<UINT>& indices);
public:
	float _radius = 0.5f;
	int _stackCount = 10;
	int _sliceCount = 10;

	ID3D11ShaderResourceView* _texture = nullptr;

	void Awake() override;
	void Render()const override;

	void ShowUI() override;

	std::string GetComponentName() override { return "Sphere"; }

};


#endif // !_SPHERE_H_