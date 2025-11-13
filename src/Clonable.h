//=====================================================================================
// Clonable.h
// Author:Kaito Aoki
// Date:2025/11/13
//=====================================================================================

#ifndef _CLONABLE_H
#define _CLONABLE_H

#include <memory>

template <typename Derivered, typename Base>
class Clonable : public Base{
public:
	std::shared_ptr<Base> Clone() const{
		static_assert(std::is_base_of_v<Base, Derivered>, "Derived must inherit from Base");
		//自身のコピーを返す
		return std::make_shared<Derivered>(static_cast<const Derivered&>(*this));
	}
};

#endif //_CLONABLE_H