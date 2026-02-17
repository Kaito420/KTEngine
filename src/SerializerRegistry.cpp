//=====================================================================================
// SerializerRegistry.cpp
// Author:Kaito Aoki
// Date:2026/02/12
//=====================================================================================

#include "SerializerRegistry.h"
#include "ComponentRegistry.h"

void RegisterAllComponents() {
	//ìÒèdìoò^ñhé~
	static bool initialized = false;
	if (initialized) return;
	initialized = true;

	//ComponentÇÃóÒãì
	ComponentRegistry::Register<RigidBody>("RigidBody");
	ComponentRegistry::Register<ColliderBox>("ColliderBox");
	ComponentRegistry::Register<ColliderSphere>("ColliderSphere");
	ComponentRegistry::Register<Cube>("Cube");
	ComponentRegistry::Register<Sphere>("Sphere");
	ComponentRegistry::Register<Shader>("Shader");
	ComponentRegistry::Register<Square>("Square");
	ComponentRegistry::Register<SkyDome>("SkyDome");
	ComponentRegistry::Register<ModelRenderer>("ModelRenderer");
	ComponentRegistry::Register<Particle>("Particle");
	ComponentRegistry::Register<Billboard>("Billboard");
	ComponentRegistry::Register<BillboardEffect>("BillboardEffect");
	ComponentRegistry::Register<Fade>("Fade");
	ComponentRegistry::Register<Wave>("Wave");
}