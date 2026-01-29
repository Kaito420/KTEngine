#include "Block.h"
#include "Cube.h"
#include "Sphere.h"
#include "Collider.h"
#include "RigidBody.h"
#include "Piece.h"
#include "Texture.h"
#include "Shader.h"

void Block::Awake()
{
	Sphere* sphere = AddComponent<Sphere>();
	AddComponent<ColliderSphere>();
	//AddComponent<Cube>();
	//AddComponent<ColliderBox>();

	AddComponent<RigidBody>();
	_piece = AddComponent<Piece>();

	Shader* shader = AddComponent<Shader>();
	shader->SetVertexShader("Toon");
	shader->SetPixelShader("Toon");

	_transform._scale = _transform._scale * (float)_piece->_type * 0.5f;
	switch (_piece->_type) {
	case Piece::Marcury:
		sphere->_texture = Texture::Load("asset\\texture\\Marcury.png");
		break;
	case Piece::Mars:
		sphere->_texture = Texture::Load("asset\\texture\\Mars.png");
		break;
	case Piece::Venus:
		sphere->_texture = Texture::Load("asset\\texture\\Venus.png");
		break;
	case Piece::Earth:
		sphere->_texture = Texture::Load("asset\\texture\\Earth.jpg");
		break;
	case Piece::Saturn:
		sphere->_texture = Texture::Load("asset\\texture\\Saturn.png");
		break;
	case Piece::Jupiter:
		sphere->_texture = Texture::Load("asset\\texture\\Jupiter.png");
		break;
	case Piece::Sun:
		sphere->_texture = Texture::Load("asset\\texture\\Sun.png");
		break;
	case Piece::Max:
		break;
	}
}

void Block::Update()
{
	if(_transform._position.y < -10.0f){
		Destroy();
	}
}
