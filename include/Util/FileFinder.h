#pragma once

#include <string>
#include <string_view>
#include <filesystem>
#include <set>
#include <vector>

class FileFinder
{
public:
	FileFinder(std::filesystem::path root);

	void AddIgnoreDirectory(const std::string& extension);

	void AddFindExtension(const std::string& extension);

	std::vector<std::string> FindFiles();

private:
	void CollectFile(std::filesystem::path directoryPath, std::vector<std::string>& paths);

	std::filesystem::path _root;
	std::set<std::string> _ignoreDirectory;
	std::set<std::string> _findExtension;
};