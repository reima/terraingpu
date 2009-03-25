#pragma once
#include "DXUT.h"
#include "AntTweakBar.h"
#include <string>
#include <map>

// Simple key-value store for (global) configuration with ATB callbacks.
namespace Config {
  template<typename T>
  struct TypedStorage {
    static std::map<std::string, T> store;
  };

  template<typename T>
  std::map<std::string, T> TypedStorage<T>::store;

  template<typename T>
  inline const T &Set(const std::string &key, const T &value) {
    TypedStorage<T>::store[key] = value;
    return value;
  }
  template<typename T>
  inline const T &Get(const std::string &key) {
    return TypedStorage<T>::store[key];
  }
  template<typename T>
  void TW_CALL GetCallback(void *value, void *clientData) {
    std::string *key = (std::string *)clientData;
    *(T *)value = TypedStorage<T>::store[*key];
  }
  template<typename T>
  void TW_CALL SetCallback(const void *value, void *clientData) {
    std::string *key = (std::string *)clientData;
    TypedStorage<T>::store[*key] = *(T *)value;
  }
  template<typename T>
  const std::string &GetKey(const std::string &key) {
    std::map<std::string, T>::const_iterator it =
        TypedStorage<T>::store.find(key);
    return it->first;
  }
};
