// vim600: fdm=marker
/* -*- c++ -*- */
///////////////////////////////////////////
// Warden
// -------------------------------------
// file       : warden.cpp
// author     : Ben Kietzman
// begin      : 2021-04-06
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

/*! \file warden.cpp
* \brief Warden Daemon
*
* Provides socket-level access to modules.
*/
// {{{ includes
#include <cerrno>
#include <clocale>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fcntl.h>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <poll.h>
#include <sstream>
#include <string>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>
using namespace std;
#include <Central>
#include <Json>
#include <SignalHandling>
#include <Storage>
#include <Syslog>
using namespace common;
// }}}
// {{{ defines
#ifdef VERSION
#undef VERSION
#endif
/*! \def VERSION
* \brief Contains the application version number.
*/
#define VERSION "0.1"
/*! \def mUSAGE(A)
* \brief Prints the usage statement.
*/
#define mUSAGE(A) cout << endl << "Usage:  "<< A << " [options]"  << endl << endl << " -d, --daemon" << endl << "     Turns the process into a daemon." << endl << endl << "     --data" << endl << "     Sets the data directory." << endl << endl << " -e EMAIL, --email=EMAIL" << endl << "     Provides the email address for default notifications." << endl << endl << " -h, --help" << endl << "     Displays this usage screen." << endl << endl << "           --max-buffer=[MAX BUFFER]" << endl << "     Provides the maximum input buffer limit in MBs." << endl << endl << "           --max-lines=[MAX LINES]" << endl << "     Provides the maximum input lines limit." << endl << endl << "     --syslog" << endl << "     Enables syslog." << endl << endl << " -v, --version" << endl << "     Displays the current version of this software." << endl << endl
/*! \def mVER_USAGE(A,B)
* \brief Prints the version number.
*/
#define mVER_USAGE(A,B) cout << endl << A << " Version: " << B << endl << endl
/*! \def CHILD_TIMEOUT
* \brief Supplies the child timeout.
*/
#define CHILD_TIMEOUT 3600
/*! \def MODULE_CONFIG
* \brief Contains the module configuration path.
*/
#define MODULE_CONFIG "/modules.conf"
/*! \def PID
* \brief Contains the PID path.
*/
#define PID "/.pid"
/*! \def START
* \brief Contains the start path.
*/
#define START "/.start"
/*! \def UNIX_SOCKET
* \brief Contains the unix socket path.
*/
#define UNIX_SOCKET "/socket"
#define PARENT_READ  readpipe[0]
#define CHILD_WRITE  readpipe[1]
#define CHILD_READ   writepipe[0]
#define PARENT_WRITE writepipe[1]
// }}}
// {{{ structs
struct connection
{
  int readpipe;
  int writepipe;
  pid_t childPid;
  string strBuffer[2];
  string strCommand;
  time_t CStartTime;
  time_t CEndTime;
  time_t CTimeout;
  Json *ptRequest;
};
// }}}
// {{{ global variables
extern char **environ;
static bool gbDaemon = false; //!< Global daemon variable.
static bool gbShutdown = false; //!< Global shutdown variable.
static string gstrApplication = "Warden"; //!< Global application name.
static string gstrData = "/data/warden"; //!< Global data path.
static string gstrEmail; //!< Global notification email address.
static Central *gpCentral = NULL; //!< Contains the Central class.
static Storage *gpStorage = NULL; //!< Contains the Storage class.
static Syslog *gpSyslog = NULL; //!< Contains the Syslog class.
// }}}
// {{{ prototypes
/*! \fn void sighandle(const int nSignal)
* \brief Establishes signal handling for the application.
* \param nSignal Contains the caught signal.
*/
void sighandle(const int nSignal);
// }}}
// {{{ main()
/*! \fn int main(int argc, char *argv[])
* \brief This is the main function.
* \return Exits with a return code for the operating system.
*/
int main(int argc, char *argv[])
{
  string strError, strPrefix = "main()";
  stringstream ssMessage;

  gpCentral = new Central(strError);
  // {{{ set signal handling
  sethandles(sighandle);
  sigignore(SIGBUS);
  sigignore(SIGCHLD);
  sigignore(SIGCONT);
  sigignore(SIGPIPE);
  sigignore(SIGSEGV);
  sigignore(SIGWINCH);
  // }}}
  // {{{ command line arguments
  for (int i = 1; i < argc; i++)
  {
    string strArg = argv[i];
    if (strArg == "-d" || strArg == "--daemon")
    {
      gbDaemon = true;
    }
    else if (strArg.size() > 7 && strArg.substr(0, 7) == "--data=")
    {
      gstrData = strArg.substr(7, strArg.size() - 7);
      gpCentral->manip()->purgeChar(gstrData, gstrData, "'");
      gpCentral->manip()->purgeChar(gstrData, gstrData, "\"");
    }
    else if (strArg == "-e" || (strArg.size() > 8 && strArg.substr(0, 8) == "--email="))
    {
      if (strArg == "-e" && i + 1 < argc && argv[i+1][0] != '-')
      {
        gstrEmail = argv[++i];
      }
      else
      {
        gstrEmail = strArg.substr(8, strArg.size() - 8);
      }
      gpCentral->manip()->purgeChar(gstrEmail, gstrEmail, "'");
      gpCentral->manip()->purgeChar(gstrEmail, gstrEmail, "\"");
    }
    else if (strArg == "-h" || strArg == "--help")
    {
      mUSAGE(argv[0]);
      return 0;
    }
    else if (strArg == "--syslog")
    {
      gpSyslog = new Syslog(gstrApplication, "warden");
    }
    else if (strArg == "-v" || strArg == "--version")
    {
      mVER_USAGE(argv[0], VERSION);
      return 0;
    }
    else
    {
      cout << endl << "Illegal option, '" << strArg << "'." << endl;
      mUSAGE(argv[0]);
      return 0;
    }
  }
  // }}}
  gpCentral->setApplication(gstrApplication);
  gpCentral->setEmail(gstrEmail);
  gpCentral->setLog(gstrData, "warden_", "daily", true, true);
  gpCentral->setRoom("#system");
  // {{{ normal run
  if (!gstrEmail.empty())
  {
    if (!gbShutdown)
    {
      int fdUnix = -1, nReturn;
      if (gbDaemon)
      {
        gpCentral->utility()->daemonize();
      }
      setlocale(LC_ALL, "");
      ofstream outPid((gstrData + PID).c_str());
      if (outPid.good())
      {
        outPid << getpid() << endl;
      }
      outPid.close();
      ofstream outStart((gstrData + START).c_str());
      outStart.close();
      gpCentral->file()->remove((gstrData + UNIX_SOCKET).c_str());
      if ((fdUnix = socket(AF_UNIX, SOCK_STREAM, 0)) >= 0)
      {
        sockaddr_un addr;
        ssMessage.str("");
        ssMessage << strPrefix << "->socket():  Created the socket.";
        gpCentral->log(ssMessage.str());
        memset(&addr, 0, sizeof(sockaddr_un));
        addr.sun_family = AF_UNIX;
        strncpy(addr.sun_path, (gstrData + UNIX_SOCKET).c_str(), sizeof(addr.sun_path) - 1);
        if (bind(fdUnix, (sockaddr *)&addr, sizeof(sockaddr_un)) == 0)
        {
          ssMessage.str("");
          ssMessage << strPrefix << "->bind():  Bound to the socket.";
          gpCentral->log(ssMessage.str());
          chmod((gstrData + UNIX_SOCKET).c_str(), 00770);
          if (listen(fdUnix, SOMAXCONN) == 0)
          {
            int fdData;
            string strSystem;
            sockaddr_un cli_addr;
            socklen_t clilen = sizeof(cli_addr);
            time_t ulModifyTime = 0;
            map<string, map<string, string> > module;
            ssMessage.str("");
            ssMessage << strPrefix << "->listen():  Listening to the socket.";
            gpCentral->log(ssMessage.str());
            clilen = sizeof(cli_addr);
            gpStorage = new Storage;
            while (!gbShutdown && (fdData = accept(fdUnix, (struct sockaddr *)&cli_addr, &clilen)) >= 0)
            {
              if (gpCentral->file()->fileExist(gstrData + "/.shutdown"))
              {
                gpCentral->file()->remove(gstrData + "/.shutdown");
                gbShutdown = true;
              }
              // {{{ reload module configuration
              if (!gpCentral->file()->fileExist(gstrData + (string)"/.lock"))
              {
                struct stat tStat;
                if (stat((gstrData + MODULE_CONFIG).c_str(), &tStat) == 0)
                {
                  if (ulModifyTime != tStat.st_mtime)
                  {
                    ifstream inFile;
                    ssMessage.str("");
                    ssMessage << strPrefix << ":  " << ((ulModifyTime == 0)?"L":"Rel") << "oaded the configuration file.";
                    gpCentral->log(ssMessage.str());
                    inFile.open((gstrData + MODULE_CONFIG).c_str());
                    if (inFile.good())
                    {
                      string strConf;
                      ulModifyTime = tStat.st_mtime;
                      module.clear();
                      while (getline(inFile, strConf).good())
                      {
                        map<string, string> moduleMap;
                        Json *ptJson = new Json(strConf);
                        ptJson->flatten(moduleMap, true, false);
                        delete ptJson;
                        if (!moduleMap.empty())
                        {
                          if (moduleMap.find("Module") != moduleMap.end() && !moduleMap["Module"].empty())
                          {
                            if (moduleMap.find("Command") != moduleMap.end() && !moduleMap["Command"].empty())
                            {
                              module[moduleMap["Module"]] = moduleMap;
                            }
                            else
                            {
                              ssMessage.str("");
                              ssMessage << strPrefix << " [" << gstrData << MODULE_CONFIG << "]:  Invalid module configuration.  Please provide the Command. --- " << strConf;
                              gpCentral->notify(ssMessage.str());
                            }
                          }
                          else
                          {
                            ssMessage.str("");
                            ssMessage << strPrefix << " [" << gstrData << MODULE_CONFIG << "]:  Invalid module configuration.  Please provide the Module. --- " << strConf;
                            gpCentral->notify(ssMessage.str());
                          }
                        }
                        else
                        {
                          ssMessage.str("");
                          ssMessage << strPrefix << " [" << gstrData << MODULE_CONFIG << "]:  Invalid JSON formatting in module configuration. --- " << strConf;
                          gpCentral->notify(ssMessage.str());
                        }
                        moduleMap.clear();
                      }
                    }
                    else
                    {
                      ssMessage.str("");
                      ssMessage << strPrefix << " [" << gstrData << MODULE_CONFIG << "]:  Unable to open module configuration for reading.";
                      gpCentral->alert(ssMessage.str());
                    }
                    inFile.close();
                  }
                }
                else
                {
                  ssMessage.str("");
                  ssMessage << strPrefix << " [" << gstrData << MODULE_CONFIG << "]:  Unable to locate module configuration.";
                  gpCentral->alert(ssMessage.str());
                }
              }
              // }}}
              if (fork() == 0)
              {
                bool bExit = false;
                char szBuffer[65536];
                list<connection *> queue;
                pollfd *fds;
                size_t unPosition, unfdSize;
                string strBuffer[2];
                while (!bExit)
                {
                  fds = new pollfd[(queue.size() * 2) + 1];
                  unfdSize = 0;
                  fds[unfdSize].fd = fdData;
                  fds[unfdSize].events = POLLIN;
                  if (!strBuffer[1].empty())
                  {
                    fds[unfdSize].events |= POLLOUT;
                  }
                  unfdSize++;
                  for (list<connection *>::iterator i = queue.begin(); i != queue.end(); i++)
                  {
                    fds[unfdSize].fd = (*i)->readpipe;
                    fds[unfdSize].events = POLLIN;
                    unfdSize++;
                    fds[unfdSize].fd = -1;
                    if (!(*i)->strBuffer[1].empty())
                    {
                      fds[unfdSize].fd = (*i)->writepipe;
                      fds[unfdSize].events = POLLOUT;
                    }
                    unfdSize++;
                  }
                  if ((nReturn = poll(fds, unfdSize, 2000)) > 0)
                  {
                    for (size_t unfdIndex = 0; unfdIndex < unfdSize; unfdIndex++)
                    {
                      list<list<connection *>::iterator> removeList;
                      // {{{ client socket
                      if (fds[unfdIndex].fd == fdData)
                      {
                        // {{{ read
                        if (fds[unfdIndex].revents & POLLIN)
                        {
                          if ((nReturn = read(fdData, szBuffer, 65536)) > 0)
                          {
                            strBuffer[0].append(szBuffer, nReturn);
                            while ((unPosition = strBuffer[0].find("\n")) != string::npos)
                            {
                              string strError;
                              Json *ptRequest = new Json(strBuffer[0].substr(0, unPosition));
                              strBuffer[0].erase(0, (unPosition + 1));
                              if (ptRequest->m.find("Module") != ptRequest->m.end() && !ptRequest->m["Module"]->v.empty())
                              {
                                string strCommand;
                                if (module.find(ptRequest->m["Module"]->v) != module.end())
                                {
                                  strCommand = module[ptRequest->m["Module"]->v]["Command"];
                                }
                                if (!strCommand.empty())
                                {
                                  char *args[100], *pszArgument;
                                  int readpipe[2] = {-1, -1}, writepipe[2] = {-1, -1};
                                  pid_t childPid;
                                  string strArgument;
                                  stringstream ssCommand;
                                  time_t CStartTime = 0, CEndTime = 0, CTimeout = CHILD_TIMEOUT;
                                  unsigned int unIndex = 0;
                                  time(&CStartTime);
                                  if (ptRequest->m.find("Timeout") != ptRequest->m.end() && !ptRequest->m["Timeout"]->v.empty())
                                  {
                                    bool bNumeric = true;
                                    for (unsigned int i = 0; i < ptRequest->m["Timeout"]->v.size(); i++)
                                    {
                                      if (!isdigit(ptRequest->m["Timeout"]->v[i]))
                                      {
                                        bNumeric = false;
                                      }
                                    }
                                    if (bNumeric)
                                    {
                                      CTimeout = atoi(ptRequest->m["Timeout"]->v.c_str());
                                    }
                                  }
                                  ssCommand.str(strCommand);
                                  while (ssCommand >> strArgument)
                                  {
                                    pszArgument = new char[strArgument.size() + 1];
                                    strcpy(pszArgument, strArgument.c_str());
                                    args[unIndex++] = pszArgument;
                                  }
                                  args[unIndex] = NULL;
                                  if (pipe(readpipe) == 0)
                                  {
                                    if (pipe(writepipe) == 0)
                                    {
                                      if ((childPid = fork()) == 0)
                                      {
                                        close(PARENT_WRITE);
                                        close(PARENT_READ);
                                        dup2(CHILD_READ, 0);
                                        close(CHILD_READ);
                                        dup2(CHILD_WRITE, 1);
                                        close(CHILD_WRITE);
                                        if (gpSyslog != NULL)
                                        {
                                          gpSyslog->commandLaunched(strCommand);
                                        }
                                        execve(args[0], args, environ);
                                        _exit(1);
                                      }
                                      else if (childPid > 0)
                                      {
                                        list<string> keys;
                                        string strJson, strSubError;
                                        Json *ptData = new Json;
                                        connection *ptConnection = new connection;
                                        close(CHILD_READ);
                                        close(CHILD_WRITE);
                                        ptConnection->readpipe = PARENT_READ;
                                        ptConnection->writepipe = PARENT_WRITE;
                                        ptConnection->ptRequest = new Json(ptRequest);
                                        keys.push_back("modules");
                                        keys.push_back(ptRequest->m["Module"]->v);
                                        if (gpStorage->request("retrieve", keys, ptData, strSubError))
                                        {
                                          ptConnection->ptRequest->insert("_storage", ptData);
                                        }
                                        keys.clear();
                                        delete ptData;
                                        ptConnection->childPid = childPid;
                                        ptConnection->CStartTime = CStartTime;
                                        ptConnection->CEndTime = CEndTime;
                                        ptConnection->CTimeout = CTimeout;
                                        ptConnection->strBuffer[1] = ptConnection->ptRequest->json(strJson) + "\n";
                                        ptConnection->strCommand = strCommand;
                                        queue.push_back(ptConnection);
                                      }
                                      else
                                      {
                                        ssMessage.str("");
                                        ssMessage << "fork(" << errno << ") " << strerror(errno);
                                        strError = ssMessage.str();
                                      }
                                    }
                                    else
                                    {
                                      ssMessage.str("");
                                      ssMessage << "pipe(write," << errno << ") " << strerror(errno);
                                      strError = ssMessage.str();
                                    }
                                  }
                                  else
                                  {
                                    ssMessage.str("");
                                    ssMessage << "pipe(read," << errno << ") " << strerror(errno);
                                    strError = ssMessage.str();
                                  }
                                  for (unsigned int i = 0; i < unIndex; i++)
                                  {
                                    delete[] args[i];
                                  }
                                }
                                else
                                {
                                  strError = "The requested module does not exist.";
                                }
                              }
                              else
                              {
                                strError = "Please provide the Module.";
                              }
                              if (!strError.empty())
                              {
                                string strJson;
                                ptRequest->insert("Status", "error");
                                ptRequest->insert("Error", strError);
                                ptRequest->json(strJson);
                                strJson += "\n";
                                strBuffer[1].append(strJson);
                              }
                              delete ptRequest;
                            }
                          }
                          else
                          {
                            bExit = true;
                          }
                        }
                        // }}}
                        // {{{ write
                        if (fds[unfdIndex].revents & POLLOUT)
                        {
                          if ((nReturn = write(fdData, strBuffer[1].c_str(), strBuffer[1].size())) > 0)
                          {
                            strBuffer[1].erase(0, nReturn);
                          }
                          else
                          {
                            bExit = true;
                          }
                        }
                        // }}}
                      }
                      // }}}
                      // {{{ module pipes
                      for (list<connection *>::iterator i = queue.begin(); i != queue.end(); i++)
                      {
                        bool bDone = false;
                        string strError;
                        // {{{ read
                        if (fds[unfdIndex].fd == (*i)->readpipe && (fds[unfdIndex].revents & (POLLHUP | POLLIN)))
                        {
                          char szBuffer[65536];
                          ssize_t nSubReturn;
                          if ((nSubReturn = read((*i)->readpipe, szBuffer, 65536)) > 0)
                          {
                            (*i)->strBuffer[0].append(szBuffer, nSubReturn);
                          }
                          else
                          {
                            bDone = true;
                            if (nSubReturn < 0)
                            {
                              stringstream ssError;
                              ssError << "read(" << errno << ") " << strerror(errno);
                              strError = ssError.str();
                            }
                          }
                        }
                        // }}}
                        // {{{ write
                        if (fds[unfdIndex].fd == (*i)->writepipe && (fds[unfdIndex].revents & (POLLHUP | POLLOUT)))
                        {
                          ssize_t nSubReturn;
                          if ((nSubReturn = write((*i)->writepipe, (*i)->strBuffer[1].c_str(), (*i)->strBuffer[1].size())) > 0)
                          {
                            (*i)->strBuffer[1].erase(0, nSubReturn);
                          }
                          else
                          {
                            bDone = true;
                            if (nSubReturn < 0)
                            {
                              stringstream ssError;
                              ssError << "write(" << errno << ") " << strerror(errno);
                              strError = ssError.str();
                            }
                          }
                        }
                        // }}}
                        time(&((*i)->CEndTime));
                        if (((*i)->CEndTime - (*i)->CStartTime) > (*i)->CTimeout)
                        {
                          bDone = true;
                          strError = "Request timed out.";
                          kill((*i)->childPid, SIGTERM);
                        }
                        // {{{ done
                        if (bDone)
                        {
                          string strJson;
                          Json *ptResponse;
                          close((*i)->readpipe);
                          close((*i)->writepipe);
                          if (gpSyslog != NULL)
                          {
                            gpSyslog->commandEnded((*i)->strCommand);
                          }
                          ptResponse = new Json((*i)->strBuffer[0]);
                          if (ptResponse->m.find("_storage") != ptResponse->m.end())
                          {
                            if ((*i)->ptRequest->m.find("Module") != (*i)->ptRequest->m.end() && !(*i)->ptRequest->m["Module"]->v.empty())
                            {
                              list<string> keys;
                              string strSubError;
                              keys.push_back("modules");
                              keys.push_back((*i)->ptRequest->m["Module"]->v);
                              gpStorage->request("add", keys, ptResponse->m["_storage"], strSubError);
                              keys.clear();
                            }
                            delete ptResponse->m["_storage"];
                            ptResponse->m.erase("_storage");
                          }
                          strBuffer[1].append(ptResponse->json(strJson)+"\n");
                          (*i)->strBuffer[0].clear();
                          ssMessage.str("");
                          ssMessage << strPrefix << " [Module:" << (*i)->ptRequest->m["Module"]->v << ",Duration:" << ((*i)->CEndTime - (*i)->CStartTime) << "]:  ";
                          if ((*i)->ptRequest->m.find("_storage") != (*i)->ptRequest->m.end())
                          {
                            delete (*i)->ptRequest->m["_storage"];
                            (*i)->ptRequest->m.erase("_storage");
                          }
                          if (!strError.empty())
                          {
                            (*i)->ptRequest->insert("Error", strError);
                          }
                          if ((*i)->ptRequest->m.find("Password") != (*i)->ptRequest->m.end())
                          {
                            (*i)->ptRequest->insert("Password", "******");
                          }
                          ssMessage << (*i)->ptRequest;
                          gpCentral->log(ssMessage.str());
                          delete (*i)->ptRequest;
                          delete *i;
                          removeList.push_back(i);
                        }
                        // }}}
                      }
                      // }}}
                      for (list<list<connection *>::iterator>::iterator i = removeList.begin(); i != removeList.end(); i++)
                      {
                        queue.erase(*i);
                      }
                      removeList.clear();
                    }
                  }
                  else if (nReturn < 0)
                  {
                    bExit = true;
                    ssMessage.str("");
                    ssMessage << strPrefix << "->poll(" << errno << ") error:  " << strerror(errno);
                    gpCentral->log(ssMessage.str());
                  }
                  delete[] fds;
                }
                for (list<connection *>::iterator i = queue.begin(); i != queue.end(); i++)
                {
                  close((*i)->readpipe);
                  close((*i)->writepipe);
                  (*i)->strBuffer[0].clear();
                  (*i)->strBuffer[1].clear();
                  delete (*i)->ptRequest;
                  delete *i;
                  ssMessage.str("");
                  ssMessage << strPrefix << ":  Removed unaccounted for request from queue.";
                  gpCentral->log(ssMessage.str());
                }
                queue.clear();
                close(fdData);
                _exit(1);
              }
              else
              {
                close(fdData);
              }
            }
            if (!gbShutdown)
            {
              gbShutdown = true;
            }
            delete gpStorage;
            ssMessage.str("");
            ssMessage << strPrefix << ":  Lost connection to the socket!  Exiting...";
            gpCentral->alert(ssMessage.str());
          }
          else
          {
            ssMessage.str("");
            ssMessage << strPrefix << "->listen(" << errno << ") error:  " << strerror(errno);
            gpCentral->alert(ssMessage.str());
          }
          close(fdUnix);
          ssMessage.str("");
          ssMessage << strPrefix << ":  Closed the socket.";
          gpCentral->log(ssMessage.str());
        }
        else
        {
          ssMessage.str("");
          ssMessage << strPrefix << "->bind(" << errno << ") error:  " << strerror(errno);
          gpCentral->alert(ssMessage.str());
        }
      }
      else
      {
        ssMessage.str("");
        ssMessage << strPrefix << "->socket(" << errno << ") error:  " << strerror(errno);
        gpCentral->alert(ssMessage.str());
      }
      gpCentral->file()->remove((gstrData + UNIX_SOCKET).c_str());
      // {{{ check pid file
      if (gpCentral->file()->fileExist((gstrData + PID).c_str()))
      {
        gpCentral->file()->remove((gstrData + PID).c_str());
      }
      // }}}
    }
  }
  // }}}
  // {{{ usage statement
  else
  {
    mUSAGE(argv[0]);
  }
  // }}}
  if (gpSyslog != NULL)
  {
    delete gpSyslog;
  }
  delete gpCentral;

  return 0;
}
// }}}
// {{{ sighandle()
void sighandle(const int nSignal)
{
  string strError, strSignal;
  stringstream ssSignal;

  sethandles(sigdummy);
  gbShutdown = true;
  if (nSignal != SIGINT && nSignal != SIGTERM)
  {
    ssSignal << nSignal;
    gpCentral->alert((string)"The program's signal handling caught a " + (string)sigstring(strSignal, nSignal) + (string)"(" + ssSignal.str() + (string)")!  Exiting...", strError);
  }
  exit(1);
}
// }}}
