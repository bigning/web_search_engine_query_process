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

    PostingInfo(int doc_id_, int freq_, std::vector<int>& pos_):
        doc_id(doc_id_), freq(freq_), pos(pos_) {}
};

struct PostingList {
    std::vector<PostingInfo> postings;
    int current;
};

struct Query {
    bool is_conjunctive;
    std::vector<std::string> words;

    Query(bool is_conjunctive_, std::vector<std::string>& words_):
    is_conjunctive(is_conjunctive_), words(words_){}
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
    int next_geq(PostingList& posting_list, int k);
    int get_freq(PostingList& posting_list);

    Query get_query();
};
