/**
 *
 *  create_project.cc
 *  An Tao
 *
 *  Copyright 2018, An Tao.  All rights reserved.
 *  https://github.com/an-tao/drogon
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Drogon
 *
 */

#include "create_project.h"
#include <drogon/DrTemplateBase.h>
#include <iostream>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <fstream>

using namespace drogon_ctl;

void create_project::handleCommand(std::vector<std::string> &parameters)
{
    if (parameters.size() < 1)
    {
        std::cout << "please input project name" << std::endl;
        exit(1);
    }
    auto pName = parameters[0];
    createProject(pName);
}
static void newCmakeFile(std::ofstream &cmakeFile,
                         const std::string &projectName)
{
    HttpViewData data;
    data.insert("ProjectName", projectName);
    auto templ = DrTemplateBase::newTemplate("cmake.csp");
    cmakeFile << templ->genText(data);
}
static void newMainFile(std::ofstream &mainFile)
{
    auto templ = DrTemplateBase::newTemplate("demoMain");
    mainFile << templ->genText();
}
static void newGitIgFile(std::ofstream &gitFile)
{
    auto templ = DrTemplateBase::newTemplate("gitignore.csp");
    gitFile << templ->genText();
}

static void newUuidFindFile(std::ofstream &uuidFile)
{
    auto templ = DrTemplateBase::newTemplate("FindUUID.csp");
    uuidFile << templ->genText();
}

static void newJsonFindFile(std::ofstream &jsonFile)
{
    auto templ = DrTemplateBase::newTemplate("FindJsoncpp.csp");
    jsonFile << templ->genText();
}

static void newMySQLFindFile(std::ofstream &mysqlFile)
{
    auto templ = DrTemplateBase::newTemplate("FindMySQL.csp");
    mysqlFile << templ->genText();
}

static void newSQLite3FindFile(std::ofstream &sqlite3File)
{
    auto templ = DrTemplateBase::newTemplate("FindSQLite3.csp");
    sqlite3File << templ->genText();
}

static void newConfigFile(std::ofstream &configFile)
{
    auto templ = DrTemplateBase::newTemplate("config");
    configFile << templ->genText();
}
static void newModelConfigFile(std::ofstream &configFile)
{
    auto templ = DrTemplateBase::newTemplate("model_json");
    configFile << templ->genText();
}
void create_project::createProject(const std::string &projectName)
{
    if (access(projectName.data(), 0) == 0)
    {
        std::cerr
            << "The directory already exists, please use another project name!"
            << std::endl;
        exit(1);
    }
    std::cout << "create a project named " << projectName << std::endl;
    mkdir(projectName.data(), 0755);
    // 1.create CMakeLists.txt
    auto r = chdir(projectName.data());
    (void)(r);
    std::ofstream cmakeFile("CMakeLists.txt", std::ofstream::out);
    newCmakeFile(cmakeFile, projectName);
    std::ofstream mainFile("main.cc", std::ofstream::out);
    newMainFile(mainFile);
    mkdir("views", 0755);
    mkdir("controllers", 0755);
    mkdir("filters", 0755);
    mkdir("plugins", 0755);
    mkdir("build", 0755);
    mkdir("models", 0755);
    mkdir("cmake_modules", 0755);
    std::ofstream jsonFile("cmake_modules/FindJsoncpp.cmake",
                           std::ofstream::out);
    newJsonFindFile(jsonFile);
    std::ofstream uuidFile("cmake_modules/FindUUID.cmake", std::ofstream::out);
    newUuidFindFile(uuidFile);
    std::ofstream mysqlFile("cmake_modules/FindMySQL.cmake",
                            std::ofstream::out);
    newMySQLFindFile(mysqlFile);
    std::ofstream sqlite3File("cmake_modules/FindSQLite3.cmake",
                              std::ofstream::out);
    newSQLite3FindFile(sqlite3File);

    std::ofstream gitFile(".gitignore", std::ofstream::out);
    newGitIgFile(gitFile);
    std::ofstream configFile("config.json", std::ofstream::out);
    newConfigFile(configFile);
    std::ofstream modelConfigFile("models/model.json", std::ofstream::out);
    newModelConfigFile(modelConfigFile);
}
