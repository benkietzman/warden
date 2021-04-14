// vim600: fdm=marker
/* -*- c++ -*- */
///////////////////////////////////////////
// Warden
// -------------------------------------
// file       : vault.cpp
// author     : Ben Kietzman
// begin      : 2021-04-09
// copyright  : kietzman.org
// email      : ben@kietzman.org
///////////////////////////////////////////

/**************************************************************************
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
*                                                                         *
**************************************************************************/

/*! \file vault.cpp
* \brief Vault Import/Export Utility
*
* Provides import/export functionality to/from the vault module.
*/
// {{{ includes
#include <cerrno>
#include <clocale>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
using namespace std;
#include <Json>
#include <Warden>
using namespace common;
// }}}
// {{{ main()
/*! \fn int main(int argc, char *argv[])
* \brief This is the main function.
* \return Exits with a return code for the operating system.
*/
int main(int argc, char *argv[])
{

  if (argc >= 4 && ((string)argv[1] == "import" || (string)argv[1] == "export" || (string)argv[1] == "remove"))
  {
    string strError;
    Warden warden(argv[3], argv[2], strError);
    if (strError.empty())
    {
      if ((string)argv[1] == "import")
      {
        string strLine;
        stringstream ssJson;
        if (argc == 5)
        {
          ifstream inJson;
          inJson.open(argv[4]);
          if (inJson)
          {
            while (getline(inJson, strLine))
            {
              ssJson << strLine;
            }
          }
          else
          {
            cerr << "ifstream::open(" << errno << ") " << strerror(errno) << endl;
          }
          inJson.close();
        }
        else
        {
          while (getline(cin, strLine))
          {
            ssJson << strLine;
          }
        }
        if (!ssJson.str().empty())
        {
          Json *ptJson = new Json(ssJson.str());
          if (warden.vaultAdd(ptJson, strError))
          {
            cout << "Warden::vaultAdd():  Imported JSON into vault." << endl;
          }
          else
          {
            cerr << "Warden::vaultAdd() error:  " << strError << endl;
          }
          delete ptJson;
        }
        else
        {
          cerr << "Please profide the JSON input data." << endl;
        }
      }
      else if ((string)argv[1] == "export")
      {
        Json *ptJson = new Json;
        if (warden.vaultRetrieve(ptJson, strError))
        {
          if (argc == 5)
          {
            ofstream outJson;
            outJson.open(argv[4]);
            if (outJson)
            {
              outJson << ptJson << endl;
            }
            else
            {
              cerr << "ofstream::open(" << errno << ") error:  " << strerror(errno) << endl;
            }
            outJson.close();
          }
          else
          {
            cout << ptJson << endl;
          }
        }
        else
        {
          cerr << "Warden::vaultRetrieve() error:  " << strError << endl;
        }
        delete ptJson;
      }
      else if (warden.vaultRemove(strError))
      {
        cout << "The vault has been removed." << endl;
      }
      else
      {
        cerr << "Warden::vaultRemove() error:  " << strError << endl;
      }
    }
    else
    {
      cerr << "Warden::Warden() error:  " << strError << endl;
    }
  }
  else
  {
    cerr << "USAGE:  " << argv[0] << " [import|export|remove] [unix socket] [application] [file|stdin]" << endl;
  }

  return 0;
}
// }}}
