// Global registry of user-defined types.

#include "UserType.h"

std::vector<UserType>& UserTypeRegistry::Mutable()
{
    static std::vector<UserType> types;
    return types;
}

const std::vector<UserType>& UserTypeRegistry::GetAll()
{
    return Mutable();
}

const UserType* UserTypeRegistry::Find(const std::string& name)
{
    for (const UserType& type : Mutable()) {
        if (type.name == name) {
            return &type;
        }
    }
    return nullptr;
}

void UserTypeRegistry::Register(UserType type)
{
    std::vector<UserType>& types = Mutable();
    for (UserType& existing : types) {
        if (existing.name == type.name) {
            existing = std::move(type);
            return;
        }
    }
    types.push_back(std::move(type));
}

bool UserTypeRegistry::Remove(const std::string& name)
{
    std::vector<UserType>& types = Mutable();
    for (std::size_t i = 0; i < types.size(); ++i) {
        if (types[i].name == name) {
            types.erase(types.begin() + static_cast<std::ptrdiff_t>(i));
            return true;
        }
    }
    return false;
}

void UserTypeRegistry::Clear()
{
    Mutable().clear();
}
