/* -*- c++ -*- */
///////////////////////////////////////////
// Passkey Authentication
// -------------------------------------
// file       : passkey.cpp
// author     : Ben Kietzman
// begin      : 2026-05-28
// copyright  : Ben Kietzman
// email      : ben@kietzman.org
///////////////////////////////////////////
// {{{ includes
#include <cerrno>
#include <cstring>
#include <ctime>
#include <fstream>
#include <iostream>
#include <list>
#include <openssl/evp.h>
#include <sstream>
#include <string>
#include <sys/types.h>
#include <unistd.h>
using namespace std;
#include <Json>
#include <ServiceJunction>
#include <StringManip>
#include <Warden>
using namespace common;
// }}}
// {{{ main()
int main(int argc, char *argv[])
{
  bool bProcessed = false;
  string strConf, strError, strJson, strUnix;
  stringstream ssMessage;
  Json *ptJson;
  StringManip manip;

  ERR_load_crypto_strings();
  OpenSSL_add_all_algorithms();
  // {{{ command line arguments
  for (int i = 1; i < argc; i++)
  {
    string strArg = argv[i];
    if (strArg.size() > 7 && strArg.substr(0, 7) == "--conf=")
    {
      strConf = strArg.substr(7, strArg.size() - 7);
      manip.purgeChar(strConf, strConf, "'");
      manip.purgeChar(strConf, strConf, "\"");
    }
    else if (strArg.size() > 7 && strArg.substr(0, 7) == "--unix=")
    {
      strUnix = strArg.substr(7, strArg.size() - 7);
      manip.purgeChar(strUnix, strUnix, "'");
      manip.purgeChar(strUnix, strUnix, "\"");
    }
  }
  // }}}
  if (getline(cin, strJson))
  {
    size_t nAuthenticatorData, nClientDataJSON, nSignature;
    string strID, strSubError;
    uint8_t *authenticatorData = NULL, *clientDataJSON = NULL, *signature = NULL;
    ptJson = new Json(strJson);
    if (ptJson->m.find("authenticatorData") != ptJson->m.end() && !ptJson->m["authenticatorData"]->v.empty())
    {
      authenticatorData = manip.decodeBase64(ptJson->m["authenticatorData"]->v, nAuthenticatorData, strSubError);
    }
    if (ptJson->m.find("clientDataJSON") != ptJson->m.end() && !ptJson->m["clientDataJSON"]->v.empty())
    {
      clientDataJSON = manip.decodeBase64(ptJson->m["clientDataJSON"]->v, nClientDataJSON, strSubError);
    }
    if (ptJson->m.find("id") != ptJson->m.end() && !ptJson->m["id"]->v.empty())
    {
      strID = ptJson->m["id"]->v;
    }
    if (ptJson->m.find("signature") != ptJson->m.end() && !ptJson->m["signature"]->v.empty())
    {
      signature = manip.decodeBase64(ptJson->m["signature"]->v, nSignature, strSubError);
    }
    if (authenticatorData != NULL && clientDataJSON != NULL && !strID.empty() && signature != NULL)
    {
      uint8_t clientHash[SHA256_DIGEST_LENGTH];
      if (SHA256(clientDataJSON, nClientDataJSON, clientHash) != NULL)
      {
        list<string> keys;
        size_t nVerify = nAuthenticatorData + SHA256_DIGEST_LENGTH;
        uint8_t *verify = (uint8_t *)malloc(nVerify);
        Json *ptConf = new Json;
        Warden warden("Central", strUnix, strError);
        memcpy(verify, authenticatorData, nAuthenticatorData);
        memcpy((verify + nAuthenticatorData), clientHash, SHA256_DIGEST_LENGTH);
        keys.push_back("conf");
        if (warden.vaultRetrieve(keys, ptConf, strError))
        {
          if (ptConf->m.find("Database") != ptConf->m.end() && !ptConf->m["Database"]->v.empty())
          {
            if (ptConf->m.find("Database Password") != ptConf->m.end() && !ptConf->m["Database Password"]->v.empty())
            {
              if (ptConf->m.find("Database Server") != ptConf->m.end() && !ptConf->m["Database Server"]->v.empty())
              {
                if (ptConf->m.find("Database User") != ptConf->m.end() && !ptConf->m["Database User"]->v.empty())
                {
                  list<string> in, out;
                  string strValue;
                  stringstream ssQuery;
                  ServiceJunction junction(strError);
                  StringManip manip;
                  Json *ptData = new Json;
                  junction.setApplication("Warden");
                  if (!strConf.empty())
                  {
                    junction.utility()->setConfPath(strConf, strError);
                  }
                  ptData->insert("Service", "mysql");
                  ptData->insert("User", ptConf->m["Database User"]->v);
                  ptData->insert("Password", ptConf->m["Database Password"]->v);
                  ptData->insert("Server", ptConf->m["Database Server"]->v);
                  ptData->insert("Database", ptConf->m["Database"]->v);
                  ssQuery << "select a.userid, b.public_key from person a, person_passkey b where a.id = b.person_id and b.passkey_id = '" << manip.escape(strID, strValue) << "'";
                  ptData->insert("Query", ssQuery.str());
                  in.push_back(ptData->json(strJson));
                  delete ptData;
                  if (junction.request(in, out, strError))
                  {
                    if (!out.empty())
                    {
                      Json *ptStatus = new Json(out.front());
                      if (ptStatus->m.find("Status") != ptStatus->m.end() && ptStatus->m["Status"]->v == "okay")
                      {
                        if (out.size() > 1)
                        {
                          Json *ptPersonPasskey;
                          out.pop_front();
                          ptPersonPasskey = new Json(out.front());
                          if (ptPersonPasskey->m.find("public_key") != ptPersonPasskey->m.end() && !ptPersonPasskey->m["public_key"]->v.empty())
                          {
                            BIO *bio;
                            if ((bio = BIO_new(BIO_s_mem())) != NULL)
                            {
                              if (BIO_write(bio, ptPersonPasskey->m["public_key"]->v.c_str(), ptPersonPasskey->m["public_key"]->v.size()) == (ssize_t)ptPersonPasskey->m["public_key"]->v.size())
                              {
                                EVP_PKEY *pkey;
                                if ((pkey = PEM_read_bio_PUBKEY(bio, NULL, NULL, NULL)) != NULL)
                                {
                                  EVP_MD_CTX *ctx;
                                  if ((ctx = EVP_MD_CTX_new()) != NULL)
                                  {
                                    if (EVP_DigestVerifyInit(ctx, NULL, EVP_sha256(), NULL, pkey) == 1)
                                    {
                                      if (EVP_DigestVerifyUpdate(ctx, (const unsigned char *)verify, nVerify) == 1)
                                      {
                                        if (EVP_DigestVerifyFinal(ctx, (const unsigned char *)signature, nSignature) == 1)
                                        {
                                          bProcessed = true;
                                          ptJson->i("User", ptPersonPasskey->m["userid"]->v);
                                        }
                                        else
                                        {
                                          strError = "Failed verification.";
                                        }
                                      }
                                      else
                                      {
                                        strError = "Failed to set data.";
                                      }
                                    }
                                    else
                                    {
                                      strError = "Failed to initialize verify.";
                                    }
                                    EVP_MD_CTX_free(ctx);
                                  }
                                  else
                                  {
                                    strError = "Failed to initialize context.";
                                  }
                                }
                                else
                                {
                                  strError = "Failed to read public key from BIO.";
                                }
                              }
                              else
                              {
                                strError = "Failed to write BIO.";
                              }
                              BIO_free(bio);
                            }
                            else
                            {
                              strError = "Failed to initialize BIO.";
                            }
                          }
                          else
                          {
                            strError = "Public key is empty.";
                          }
                          delete ptPersonPasskey;
                        }
                        else
                        {
                          strError = "Failed to retrieve public key.";
                        }
                      }
                      else if (ptStatus->m.find("Error") != ptStatus->m.end() && !ptStatus->m["Error"]->v.empty())
                      {
                        strError = ptStatus->m["Error"]->v;
                      }
                      else
                      {
                        strError = "Encountered an unknown error.";
                      }
                      delete ptStatus;
                    }
                    else
                    {
                      strError = "Failed to receive a response.";
                    }
                  }
                  in.clear();
                  out.clear();
                }
                else
                {
                  strError = "Please provide the Database User.";
                }
              }
              else
              {
                strError = "Please provide the Database Server.";
              }
            }
            else
            {
              strError = "Please provide the Database Password.";
            }
          }
          else
          {
            strError = "Please provide the Database.";
          }
        }
        delete ptConf;
        free(verify);
      }
      else
      {
        strError = "Failed to compute the SHA-256 hash.";
      }
    }
    else
    {
      strError = "Failed authentication.";
    }
  }
  else
  {
    ptJson = new Json;
    strError = "Please provide the request.";
  }
  ptJson->insert("Status", ((bProcessed)?"okay":"error"));
  if (!strError.empty())
  {
    ptJson->insert("Error", strError);
  }
  cout << ptJson << endl;
  delete ptJson;
  ERR_free_strings();

  return 0;
}
// }}}
