// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#ifdef _WIN32
  #define NOMINMAX
  #include <windows.h>
  #include <bcrypt.h>
#else
  #include <openssl/evp.h>
#endif
#include <array>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>

namespace SMPPlaytest {
#ifdef _WIN32
class Digest {
  BCRYPT_HASH_HANDLE handle=nullptr;
  std::array<unsigned char,65536> buffer{};
  size_t used=0;
  static BCRYPT_ALG_HANDLE algorithm() {
    static BCRYPT_ALG_HANDLE value=[] {
      BCRYPT_ALG_HANDLE a=nullptr;
      if(BCryptOpenAlgorithmProvider(&a,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0)
        throw std::runtime_error("SHA256 provider unavailable");
      return a;
    }();
    return value;
  }
public:
  Digest(){reset();}
  Digest(const Digest&)=delete;
  Digest& operator=(const Digest&)=delete;
  ~Digest(){if(handle)BCryptDestroyHash(handle);}
  void reset() {
    if(handle)BCryptDestroyHash(handle);
    handle=nullptr;used=0;
    if(BCryptCreateHash(algorithm(),&handle,nullptr,0,nullptr,0,0)<0)
      throw std::runtime_error("SHA256 allocation failed");
  }
  void flush() {
    if(used && BCryptHashData(handle,buffer.data(),ULONG(used),0)<0)
      throw std::runtime_error("SHA256 update failed");
    used=0;
  }
  void add(const void* data,size_t size) {
    auto p=static_cast<const unsigned char*>(data);
    while(size) {
      auto n=std::min(size,buffer.size()-used);
      std::memcpy(buffer.data()+used,p,n);used+=n;p+=n;size-=n;
      if(used==buffer.size())flush();
    }
  }
  void word(uint64_t value) {
    unsigned char b[8];for(unsigned i=0;i<8;i++)b[i]=uint8_t(value>>(i*8));add(b,8);
  }
  std::string hex() {
    flush();BCRYPT_HASH_HANDLE copy=nullptr;
    if(BCryptDuplicateHash(handle,&copy,nullptr,0,0)<0)throw std::runtime_error("SHA256 copy failed");
    unsigned char result[32];auto status=BCryptFinishHash(copy,result,32,0);BCryptDestroyHash(copy);
    if(status<0)throw std::runtime_error("SHA256 finish failed");
    static const char digits[]="0123456789abcdef";std::string s;s.reserve(64);
    for(auto b:result){s+=digits[b>>4];s+=digits[b&15];}return s;
  }
};
#else
// EVP keeps the same incremental, non-consuming snapshot contract as BCrypt.
// In particular, taking a witness hash must not reset the live audit digest.
class Digest {
  EVP_MD_CTX* handle=nullptr;
  std::array<unsigned char,65536> buffer{};
  size_t used=0;
public:
  Digest(){reset();}
  Digest(const Digest&)=delete;
  Digest& operator=(const Digest&)=delete;
  ~Digest(){EVP_MD_CTX_free(handle);}
  void reset() {
    if(!handle)handle=EVP_MD_CTX_new();
    used=0;
    if(!handle || EVP_DigestInit_ex(handle,EVP_sha256(),nullptr)!=1)
      throw std::runtime_error("SHA256 allocation failed");
  }
  void flush() {
    if(used && EVP_DigestUpdate(handle,buffer.data(),used)!=1)
      throw std::runtime_error("SHA256 update failed");
    used=0;
  }
  void add(const void* data,size_t size) {
    auto p=static_cast<const unsigned char*>(data);
    while(size) {
      auto n=std::min(size,buffer.size()-used);
      std::memcpy(buffer.data()+used,p,n);used+=n;p+=n;size-=n;
      if(used==buffer.size())flush();
    }
  }
  void word(uint64_t value) {
    unsigned char b[8];for(unsigned i=0;i<8;i++)b[i]=uint8_t(value>>(i*8));add(b,8);
  }
  std::string hex() {
    flush();auto copy=EVP_MD_CTX_new();
    if(!copy)throw std::runtime_error("SHA256 copy allocation failed");
    unsigned char result[EVP_MAX_MD_SIZE];unsigned size=0;
    bool ok=EVP_MD_CTX_copy_ex(copy,handle)==1 && EVP_DigestFinal_ex(copy,result,&size)==1;
    EVP_MD_CTX_free(copy);
    if(!ok || size!=32)throw std::runtime_error("SHA256 finish failed");
    static const char digits[]="0123456789abcdef";std::string s;s.reserve(64);
    for(unsigned i=0;i<size;i++){auto b=result[i];s+=digits[b>>4];s+=digits[b&15];}return s;
  }
};
#endif
inline std::string sha256(const void* p,size_t n){Digest d;d.add(p,n);return d.hex();}
}
