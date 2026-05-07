#define _CRT_SECURE_NO_WARNINGS
#include<iostream>
#include<string>
#include<vector>
#include<list>
#include<unordered_map>
#include<fstream>
#include<sstream>
#include<algorithm>

#undef UNICODE
#include<Windows.h>

//本地文档搜索引擎（倒排索引）

//倒排项
struct InvertTerm
{
	InvertTerm(std::string docid, int freqs, int location)
		:docid_(docid)
		, freqs_(freqs)
	{
		location_.emplace_back(location);
	}
	bool operator==(const InvertTerm& term) const
	{
		return docid_ == term.docid_;
	}
	bool operator<(const InvertTerm& term) const
	{
		return docid_ < term.docid_;
	}
	std::string docid_;		//单词出现的文档
	int freqs_;		//单词出现的频率
	std::vector<int> location_;		//单词出现的位置
};

//倒排列表
class InvertList
{
public:
	//添加倒排项
	void addTerm(std::string docid, int location)
	{
		for (auto& term : invertlist_)
		{
			//列表中存在单词修改对应属性
			if (docid == term.docid_)
			{
				term.freqs_++;
				term.location_.emplace_back(location);
				return;
			}
		}
		invertlist_.emplace_back(docid, 1, location);
	}
	//获取倒排列表
	const std::list<InvertTerm>& getInvertList() const
	{
		return invertlist_;
	}
private:
	std::list<InvertTerm> invertlist_;		//当前单词所属的倒排列表
};

//倒排索引
class InvertIndex
{
public:
	//设置搜索路径
	void setSearchPath(std::string path)
	{
		std::cout << "文件搜索中..." << std::endl;
		getAllFile(path.c_str());
		std::cout << "搜索完成" << std::endl;
		std::cout << "创建倒排索引中..." << std::endl;
		createInvertIndex();
		std::cout << "创建完成" << std::endl;
	}
	//查询接口
	void query(std::string phrase)
	{
		//先进行句子分词
		std::vector<std::string> wordList;
		char* word = strtok(const_cast<char*>(phrase.c_str()), " ");
		while (word != nullptr)
		{
			word = trim(word);
			if (strlen(word) > 0)
			{
				wordList.emplace_back(word);
			}
			word = strtok(nullptr, " ");
		}
		if (wordList.empty())
			return;
		if (wordList.size() == 1)
		{
			auto it = invertindex_.find(wordList[0]);
			if (it == invertindex_.end())
			{
				std::cout << "未匹配到相应的内容" << std::endl;
				return;
			}
			for (auto& term : it->second.getInvertList())
			{
				std::cout << "docid：" << term.docid_ << " freqs：" << term.freqs_ << std::endl;
			}
		}
		else
		{
			//多个单词进行合并处理
			std::vector<InvertList> invertList;
			for(int i = 0; i < wordList.size(); i++)
			{
				auto it = invertindex_.find(wordList[i]);
				if (it != invertindex_.end())
				{
					invertList.emplace_back(it->second);
				}
			}
			std::vector<InvertTerm> termShared;
			std::vector<InvertTerm> v1(invertList[0].getInvertList().begin(),
				invertList[0].getInvertList().end());
			for (int j = 1; j < invertList.size(); j++)
			{
				std::vector<InvertTerm> v2(invertList[j].getInvertList().begin(),
					invertList[j].getInvertList().end());
				//求两个倒排列表里面倒排项的交集
				sort(v1.begin(), v1.end());
				sort(v2.begin(), v2.end());
				//set_intersection要传入有序的容器
				std::set_intersection(v1.begin(), v1.end(), v2.begin(), v2.end(), std::back_inserter(termShared));
				v1.swap(termShared);
				termShared.clear();
			}
			for (auto& term : v1)
			{
				std::cout << "docid：" << term.docid_ << " freqs：" << term.freqs_ << std::endl;
				std::cout << "location：";
				for (auto location : term.location_)
				{
					std::cout << location << " ";
				}
				std::cout << std::endl;
			}
		}
	}
	//设置过滤后缀
	void setSuffix(std::string suffix)
	{
		suffix_ = suffix;
	}
private:
	//搜索指定路径下的文件--win32
	int getAllFile(const char* path)
	{
		char szFind[MAX_PATH];
		WIN32_FIND_DATA FindFileData;
		strcpy_s(szFind, path);
		strcat_s(szFind, "\\*.*");
		HANDLE hFind = FindFirstFile(szFind, &FindFileData);
		if (INVALID_HANDLE_VALUE == hFind)
			return -1;
		do
		{
			if (FindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			{
				if (strcmp(FindFileData.cFileName, ".") != 0
					&& strcmp(FindFileData.cFileName, "..") != 0)
				{
					//发现子目录，递归
					char szFile[MAX_PATH] = { 0 };
					strcpy_s(szFile, path);
					strcat_s(szFile, "\\");
					strcat_s(szFile, FindFileData.cFileName);
					getAllFile(szFile);
				}
			}
			else
			{
				//找到文件
				std::string fileName(FindFileData.cFileName);
				int pos = fileName.find(suffix_);
				if (pos != std::string::npos && pos + suffix_.size() == fileName.size())
				{
					//std::cout << path << "\\" << FindFileData.cFileName << std::endl;
					std::string filePath(path);
					filePath.append("\\");
					filePath.append(fileName);
					filelist_.emplace_back(filePath);
				}			
			}
		} while (FindNextFile(hFind, &FindFileData));
		FindClose(hFind);
		return 0;
	}
	//创建倒排索引结构
	void createInvertIndex()
	{
		for (auto& filePath : filelist_)
		{
			FILE* fp = std::fopen(filePath.c_str(), "r");
			if (fp == nullptr)
			{
				std::cerr << filePath << "...打开失败" << std::endl;
				continue;
			}
			//按行读取文件里面的内容，按照" "进行分词
			std::vector<std::string> wordList;
			int location = 0;
			const int LINE_SIZE = 2048;
			char line[LINE_SIZE] = { 0 };
			while (!feof(fp))
			{
				//读取一行文件的内容
				fgets(line, LINE_SIZE, fp);
				//按照" "进行分词 split,strtok
				char* word = std::strtok(line, " ");
				while (word != nullptr)
				{
					//过滤word多余的字符
					word = trim(word);
					if (strlen(word) > 0)
					{
						wordList.emplace_back(word);
					}
					word = std::strtok(nullptr, " ");
				}
				//开始给wordList里面的单词创建或修改倒排列表
				for (auto& w : wordList)
				{
					location++;
					auto it = invertindex_.find(w);
					if (it == invertindex_.end())
					{
						//新建w单词的倒排列表
						InvertList list;
						list.addTerm(filePath, location);
						invertindex_.emplace(w, list);
					}
					else
					{
						it->second.addTerm(filePath, location);
					}
				}
			}
			fclose(fp);
		}
	}
	char* trim(char* word)
	{
		int i = 0;
		while (word[i] != '\0')
		{
			if (word[i] == ' ' || word[i] == '\t' || word[i] == '\n')
			{
				i++;
			}
			else break;
		}
		int j = i;
		while (word[j] != '\0')
		{
			if (word[j] == ' ' || word[j] == '\t' || word[j] == '\n')
			{
				break;
			}
			j++;
		}
		word[j] = '\0';
		return word + i;
	}
private:
	std::string suffix_;	//过滤文档后缀  .cpp
	std::list<std::string> filelist_;	//存储所有需要建立倒排的文件
	std::unordered_map<std::string,InvertList> invertindex_;	//索引表
};


int main()
{
	InvertIndex invertindex;
	invertindex.setSuffix(".cpp");
	invertindex.setSearchPath("D:\\studyCode\\C++\\数据结构");
	for (;;)
	{
		char buff[128] = { 0 };
		std::cout << "请输入搜索内容：";
		std::cin.getline(buff, 128);
		invertindex.query(buff);
	}

	return 0;
}