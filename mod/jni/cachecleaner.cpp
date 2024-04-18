#include <chrono>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string_view>
#include <sys/prctl.h>
#include <thread>
#include <unordered_map>

#include <CuLogger.h>
#include <CuStringMatcher.h>
#include <yaml-cpp/yaml.h>

#define eq ==

using namespace std;
using namespace literals;
using namespace CU;
namespace fs = filesystem;

template <typename T>
bool eq_and_not_eq(T a, T b, T c)
{
  return (a eq b) and (a not_eq c);
}

template <typename T>
bool all_eq(T a, T b) { return (a eq b); }

template <typename T, typename... Args>
bool all_eq(T a, T b, Args... args)
{
  return (a eq b) and all_eq(a, args...);
}

template <typename T>
bool all_not_eq(T a, T b) { return (a not_eq b); }

template <typename T, typename... Args>
bool all_not_eq(T a, T b, Args... args)
{
  return (a not_eq b) and all_not_eq(a, args...);
}

template <typename T>
bool all_or_eq(T a, T b) { return (a eq b); }

template <typename T, typename... Args>
bool all_or_eq(T a, T b, Args... args)
{
  return (a eq b) or all_or_eq(a, args...);
}

template <typename T>
bool all_or_not_eq(T a, T b) { return (a not_eq b); }

template <typename T, typename... Args>
bool all_or_not_eq(T a, T b, Args... args)
{
  return (a not_eq b) or all_or_not_eq(a, args...);
}

#define CLOGE(...) Logger::Error(__VA_ARGS__)
#define CLOGW(...) Logger::Warn(__VA_ARGS__)
#define CLOGI(...) Logger::Info(__VA_ARGS__)
#define CLOGD(...) Logger::Debug(__VA_ARGS__)
#define CLOGV(...) Logger::Verbose(__VA_ARGS__)

string exec(string_view cmd)
{
  if (FILE *fp = popen(cmd.data(), "r"))
  {
    ostringstream stm;
    char line[128];
    while (fgets(line, 128, fp))
    {
      stm << line;
    }
    pclose(fp);
    return stm.str();
  }
  return ""s;
}

void rmDir(string_view dir)
{
  try
  {
    if (fs::exists(dir) && fs::is_directory(dir))
    {
      for (const auto &path : fs::directory_iterator(dir))
      {
        fs::remove_all(path.path());
      }
    }
  }
  catch (const exception &e)
  {
    stringstream ss;
    ss << "Error while running: "sv << e.what();
    CLOGE(ss.str().c_str());
    Logger::Flush();
  }
}

void cleanApp(const string &app, bool multiUser, const string &userID = "0"s)
{
  if (multiUser)
  {
    rmDir("/storage/emulated/"s + userID + "/Android/data/"s + app + "/cache"s);
    rmDir("/data/user/"s + userID + "/"s + app + "/cache"s);
    rmDir("/data/user/"s + userID + "/"s + app + "/code_cache"s);
  }
  else
  {
    rmDir("/sdcard/Android/data/"s + app + "/cache"s);
    rmDir("/data/data/"s + app + "/cache"s);
    rmDir("/data/data/"s + app + "/code_cache"s);
  }
  CLOGI(("Cleaned app cache: "s + app).c_str());
}

void cleanDir(string_view dir, bool &cleanDotFile, vector<string> &fileWhitelist, vector<string> &filenameWhitelist, vector<string> &filenameBlacklist)
{
  this_thread::sleep_for(chrono::milliseconds(1));
  for (const auto &entry : fs::directory_iterator(dir))
  {
    bool skip = false;
    for (const string &whitelistFile : fileWhitelist)
    {
      fs::path parentPath = fs::canonical(fs::path(whitelistFile));
      fs::path filePath = fs::canonical(fs::path(dir));
      if (parentPath == filePath || mismatch(parentPath.begin(), parentPath.end(), filePath.begin()).first == parentPath.end())
      {
        CLOGI(("Skip clean file: "s + entry.path().string()).c_str());
        skip = true;
      }
    }
    if (not skip)
    {
      for (const string &whitelistFilename : filenameWhitelist)
      {
        if (entry.path().filename().string().find(whitelistFilename) not_eq
            string::npos)
        {
          // CLOGI(("Skip clean file: "s + entry.path().string()).c_str());
          skip = true;
        }
      }
      if (not skip)
      {
        if (cleanDotFile and fs::exists(entry.path().string()) and
            string_view(entry.path().filename().string().data(), 1) eq "."sv and
            string_view(entry.path().filename().string()) not_eq ".nomedia"sv)
        {
          fs::remove_all(entry.path().string());
          CLOGI(("Cleaned file: "s + entry.path().string()).c_str());
          continue;
        }
        for (const string &blacklistFilename : filenameBlacklist)
        {
          if (fs::exists(entry.path().string()) and
              entry.path().filename().string().find(blacklistFilename) not_eq string::npos)
          {
            fs::remove_all(entry.path().string());
            CLOGI(("Cleaned file: "s + entry.path().string()).c_str());
            break;
          }
        }
        if (fs::exists(entry.path().string()) and fs::is_directory(entry))
        {
          cleanDir(entry.path().string(), cleanDotFile, fileWhitelist, filenameWhitelist, filenameBlacklist);
        }
      }
    }
  }
}

signed main(int argc, char *argv[])
{
  if (argc < 2)
  {
    cout << "Invalid usage"sv << endl
         << endl
         << "Cache Cleaner(https://github.com/Cai-Ming-Yu/CMY-CacheCleaner)"sv
         << endl
         << endl
         << "Usage: "sv << argv[0] << " [yaml config file]"sv << endl;
    return -1;
  }

  prctl(PR_SET_NAME, "CacheCleaner", 0, 0, 0);
  strcpy(argv[0], "CacheCleaner");

  string configFile = fs::canonical(fs::absolute(fs::path(argv[1]))).string();
  string logPath = fs::canonical(fs::absolute(fs::path(argv[1])).parent_path()).string() + "/runtimeLog.txt"s;

  bool cleanAppCache, multiUser, cleanSdcard, cleanDotFile;
  string time, appMode, appWhitelist, appBlacklist;
  vector<string> searchExt, filenameWhitelist, filenameBlacklist, fileWhitelist, fileBlacklist;
  map<string, vector<string>> appFileBlacklist;

  map<char, int> timeUnitInSeconds = {{'s', 1}, {'m', 60}, {'h', 3600}, {'d', 86400}};

  unordered_map<string_view, int> appModes = {{"user"sv, 1}, {"system"sv, 2}, {"all"sv, 3}};

  for (;;)
  {
    Logger::Create(Logger::LogLevel::INFO, logPath);
    CLOGI(("Created log file: "s + logPath).c_str());
    Logger::Flush();

    cleanAppCache = multiUser = cleanSdcard = cleanDotFile = false;
    time = appMode = appWhitelist = appBlacklist = ""s;
    searchExt = filenameWhitelist = filenameBlacklist = fileWhitelist = fileBlacklist = {};
    appFileBlacklist = {};

    for (auto &str : {&time, &appMode, &appWhitelist, &appBlacklist})
    {
      (*str).clear();
      (*str).shrink_to_fit();
    }

    for (auto &str : {&searchExt, &filenameWhitelist, &filenameBlacklist, &fileWhitelist, &fileBlacklist})
    {
      for (auto &s : *str)
      {
        s.clear();
        s.shrink_to_fit();
      }
    }

    for (auto &pair : appFileBlacklist)
    {
      for (auto &str : pair.second)
      {
        str.clear();
        str.shrink_to_fit();
      }
    }

    try
    {
      YAML::Node config = YAML::LoadFile(configFile);
      CLOGI(("Read config file: "s + configFile).c_str());

      time = config["time"sv].as<string>();
      CLOGI(("config time: "s + time).c_str());

      cleanAppCache = config["cleanAppCache"sv].as<bool>();
      if (cleanAppCache)
      {
        CLOGI("config cleanAppCache: true");
      }
      else
      {
        CLOGI("config cleanAppCache: false");
      }

      appMode = config["appMode"sv].as<string>();
      CLOGI(("config appMode: "s + appMode).c_str());

      cleanAppCache = config["multiUser"sv].as<bool>();
      if (cleanAppCache)
      {
        CLOGI("config multiUser: true");
      }
      else
      {
        CLOGI("config multiUser: false");
      }

      CLOGI("config appWhitelist:");
      for (const auto &app : config["appWhitelist"sv])
      {
        appWhitelist += "アプリ"s + app.as<string>() + "アプリ"s;
        CLOGI(app.as<string>().c_str());
      }

      CLOGI("config appBlacklist:");
      for (const auto &app : config["appBlacklist"sv])
      {
        appBlacklist += "アプリ"s + app.as<string>() + "アプリ"s;
        CLOGI(app.as<string>().c_str());
      }

      CLOGI("config appFileBlacklist:");
      const YAML::Node &appFileBlacklistNode = config["appFileBlacklist"sv];
      if (appFileBlacklistNode.IsDefined() && appFileBlacklistNode.IsMap())
      {
        for (const auto &app : appFileBlacklistNode)
        {
          string packageName = app.first.as<string>();
          CLOGI((packageName + ":"s).c_str());
          const YAML::Node &pathsNode = app.second;
          if (pathsNode.IsSequence())
          {
            for (const auto &path : pathsNode)
            {
              appFileBlacklist[packageName].push_back(path.as<string>());
              CLOGI(path.as<string>().c_str());
            }
          }
          else
          {
            appFileBlacklist[packageName].push_back(pathsNode.as<string>());
            CLOGI(pathsNode.as<string>().c_str());
          }
        }
      }

      cleanSdcard = config["cleanSdcard"sv].as<bool>();
      if (cleanSdcard)
      {
        if (multiUser)
        {
          for (const auto &entry : fs::directory_iterator("/storage/emulated"sv))
          {
            if (entry.is_directory())
            {
              searchExt.push_back("/storage/emulated/"s + entry.path().filename().string());
            }
          }
        }
        else
        {
          searchExt.push_back("/sdcard"s);
        }
        CLOGI("config cleanSdcard: true");
      }
      else
      {
        CLOGI("config cleanSdcard: false");
      }

      CLOGI("config searchExt:");
      for (const auto &dir : config["searchExt"sv])
      {
        searchExt.push_back(dir.as<string>());
        CLOGI(dir.as<string>().c_str());
      }

      cleanDotFile = config["cleanDotFile"sv].as<bool>();
      if (cleanDotFile)
      {
        CLOGI("config cleanDotFile: true");
      }
      else
      {
        CLOGI("config cleanDotFile: false");
      }

      CLOGI("config filenameWhitelist:");
      for (const auto &filename : config["filenameWhitelist"sv])
      {
        filenameWhitelist.push_back(filename.as<string>());
        CLOGI(filename.as<string>().c_str());
      }

      CLOGI("config filenameBlacklist:");
      for (const auto &filename : config["filenameBlacklist"sv])
      {
        filenameBlacklist.push_back(filename.as<string>());
        CLOGI(filename.as<string>().c_str());
      }

      CLOGI("config fileWhitelist:");
      for (const auto &file : config["fileWhitelist"sv])
      {
        fileWhitelist.push_back(file.as<string>());
        CLOGI(file.as<string>().c_str());
      }

      CLOGI("config fileBlacklist:");
      for (const auto &file : config["fileBlacklist"sv])
      {
        fileBlacklist.push_back(file.as<string>());
        CLOGI(file.as<string>().c_str());
      }

      CLOGI("Read config finished");
      Logger::Flush();
    }
    catch (const YAML::Exception &e)
    {
      stringstream ss;
      ss << "Error while parsing YAML: "sv << e.what();
      CLOGE(ss.str().c_str());
      Logger::Flush();
      return -1;
    }

    try
    {
      if (cleanAppCache)
      {
        string apps;
        if (multiUser)
        {
          for (const auto &entry : fs::directory_iterator("/data/user"sv))
          {
            if (entry.is_directory())
            {
              string userID = entry.path().filename().string();
              switch (appModes[appMode])
              {
              case 1:
                CLOGI("Run cleanAppCache in user mode");
                apps = exec("pm list packages -3 --user "s + userID + " | sed 's/package://'"s);
                break;
              case 2:
                CLOGI("Run cleanAppCache in system mode");
                apps = exec("pm list packages -s --user "s + userID + " | sed 's/package://' | grep -v '^android$'"s);
                break;
              case 3:
                CLOGI("Run cleanAppCache in all mode");
                apps = exec("pm list packages --user "s + userID + " | sed 's/package://' | grep -v '^android$'"s);
                break;
              default:
                CLOGW("Unknown appMode, cleanAppCache will not run");
                apps = ""s;
                break;
              }
              if (all_not_eq(apps, ""s, "\n"s))
              {
                istringstream iss(apps);
                string packageName;
                while (getline(iss, packageName, '\n'))
                {
                  StringMatcher matcher("*アプリ"s + packageName + "アプリ*"s);
                  if (matcher.match(appWhitelist))
                  {
                    CLOGI(("Skip cleanup app: "s + packageName).c_str());
                  }
                  else if (matcher.match(appBlacklist))
                  {
                    CLOGI(("Force cleanup app: "s + packageName).c_str());
                    cleanApp(packageName, multiUser, userID);
                  }
                  else
                  {
                    if (all_or_eq(exec("pidof "s + packageName + " 2>/dev/null"s), ""s, "\n"s))
                    {
                      cleanApp(packageName, multiUser, userID);
                    }
                  }
                  this_thread::sleep_for(chrono::milliseconds(1));
                }
              }
            }
          }
        }
        else
        {
          switch (appModes[appMode])
          {
          case 1:
            CLOGI("Run cleanAppCache in user mode");
            apps = exec("pm list packages -3 | sed 's/package://'"sv);
            break;
          case 2:
            CLOGI("Run cleanAppCache in system mode");
            apps = exec("pm list packages -s | sed 's/package://' | grep -v '^android$'"sv);
            break;
          case 3:
            CLOGI("Run cleanAppCache in all mode");
            apps = exec("pm list packages | sed 's/package://' | grep -v '^android$'"sv);
            break;
          default:
            CLOGW("Unknown appMode, cleanAppCache will not run");
            apps = ""s;
            break;
          }
          if (all_not_eq(apps, ""s, "\n"s))
          {
            istringstream iss(apps);
            string packageName;
            while (getline(iss, packageName, '\n'))
            {
              StringMatcher matcher("*アプリ"s + packageName + "アプリ*"s);
              if (matcher.match(appWhitelist))
              {
                CLOGI(("Skip cleanup app: "s + packageName).c_str());
              }
              else if (matcher.match(appBlacklist))
              {
                CLOGI(("Force cleanup app: "s + packageName).c_str());
                cleanApp(packageName, multiUser);
              }
              else
              {
                if (all_or_eq(exec("pidof "s + packageName + " 2>/dev/null"s), ""s, "\n"s))
                {
                  cleanApp(packageName, multiUser);
                }
              }
              this_thread::sleep_for(chrono::milliseconds(1));
            }
          }
        }
      }
      Logger::Flush();

      for (auto &str : {&appMode, &appWhitelist, &appBlacklist})
      {
        (*str).clear();
        (*str).shrink_to_fit();
      }

      for (const auto &[app, file] : appFileBlacklist)
      {
        if (all_or_eq(exec("pidof "s + app + " 2>/dev/null"s), ""s, "\n"s))
        {
          for (const auto &path : file)
          {
            if (fs::exists(path))
            {
              fs::remove_all(path);
              CLOGI(("Cleaned file: "s + path).c_str());
              this_thread::sleep_for(chrono::milliseconds(1));
            }
          }
        }
      }
      Logger::Flush();

      for (auto &pair : appFileBlacklist)
      {
        for (auto &str : pair.second)
        {
          str.clear();
          str.shrink_to_fit();
        }
      }

      for (const string &dir : searchExt)
      {
        bool skip = false;
        for (const string &whitelistFile : fileWhitelist)
        {
          fs::path parentPath = fs::canonical(fs::path(whitelistFile));
          fs::path filePath = fs::canonical(fs::path(dir));
          if (parentPath == filePath ||
              mismatch(parentPath.begin(), parentPath.end(), filePath.begin()).first == parentPath.end())
          {
            // CLOGI(("Skip clean file: "s + dir).c_str());
            skip = true;
          }
        }
        if (not skip)
        {
          cleanDir(dir, cleanDotFile, fileWhitelist, filenameWhitelist, filenameBlacklist);
        }
        this_thread::sleep_for(chrono::milliseconds(1));
      }
      Logger::Flush();

      for (auto &str : {&searchExt, &filenameWhitelist, &filenameBlacklist, &fileWhitelist})
      {
        for (auto &s : *str)
        {
          s.clear();
          s.shrink_to_fit();
        }
      }

      for (const string &file : fileBlacklist)
      {
        if (fs::exists(file))
        {
          fs::remove_all(file);
          CLOGI(("Cleaned file: "s + file).c_str());
          this_thread::sleep_for(chrono::milliseconds(1));
        }
      }

      for (auto &str : {&fileBlacklist})
      {
        for (auto &s : *str)
        {
          s.clear();
          s.shrink_to_fit();
        }
      }

      CLOGI(("Work finished, rest "s + time).c_str());
      Logger::Flush();
      this_thread::sleep_for(chrono::seconds(stoi(time.c_str()) * timeUnitInSeconds[time.back()]));
    }
    catch (const exception &e)
    {
      stringstream ss;
      ss << "Error while running: "sv << e.what();
      CLOGE(ss.str().c_str());
      Logger::Flush();
      return -1;
    }
  }

  return 0;
}