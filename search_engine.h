#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <fstream>

struct WordInfo {
    std::string word;
    long long start_position;
    long long end_position;
    int num_of_doc_contain_this_word;
};

struct DocInfo {
    int doc_id;
    std::string url;
    int length;
    int start_position;

    DocInfo(int doc_id_, std::string url_, int length_, int pos_):
        doc_id(doc_id_), url(url_), length(length_), start_position(pos_) {}
};

struct PostingInfo {
    int doc_id;
    int freq;
    std::vector<int> pos;
};

struct PostingList {
    long long start;
    long long end;
    long long current;

    PostingList(long long start_, long long end_, long long current_): 
        start(start_), end(end_), current(current_) {}
    PostingList(): start(-1), end(-1), current(-1) {}
};

class SearchEngine {
public:
    SearchEngine();
    void run();
    void test();
    
private:
    std::string data_path_;
    float average_doc_length_;
    std::map<std::string, WordInfo> words_;
    std::vector<DocInfo> docs_;
    int doc_num_;
    
    std::ifstream inverted_index_file_;

    void read_in_words_and_urls();
    bool open_list(std::string& word, PostingList& posting_list);
    bool next_geq(PostingList& posting_list, int k, PostingInfo& posting_info);
    void get_current_posting_info(long long& current_pos, PostingInfo& posting_info);
};
