#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <fstream>

#define K_CACHE_SIZE 1024

struct WordInfo {
    std::string word;
    long long start_position;
    long long end_position;
    int num_of_doc_contain_this_word;

    WordInfo(std::string& word_, long long start_, long long end_, int num_):
        word(word_), start_position(start_), end_position(end_),
        num_of_doc_contain_this_word(num_) {}
    WordInfo() {}
};

struct DocInfo {
    int doc_id;
    std::string url;
    int length;
    long long start_position;

    DocInfo(int doc_id_, std::string url_, int length_, long long pos_):
        doc_id(doc_id_), url(url_), length(length_), start_position(pos_) {}
    DocInfo() {}
};

struct PostingInfo {
    int doc_id;
    int freq;
    std::vector<int> pos;

    PostingInfo(int doc_id_, int freq_, std::vector<int>& pos_):
        doc_id(doc_id_), freq(freq_), pos(pos_) {}
    PostingInfo() {};
};

struct PostingList {
    std::vector<PostingInfo> postings;
    int current;
};

struct QueryRes {
    int doc_id;
    float score;
    std::vector<std::vector<int> > pos;

    bool operator<(const QueryRes& r) const {
        return score > r.score;
    }
};

struct Query {
    bool is_conjunctive;
    std::vector<std::string> words;

    Query(bool is_conjunctive_, std::vector<std::string>& words_):
    is_conjunctive(is_conjunctive_), words(words_){}

    Query() {};
};

class SearchEngine {
public:
    SearchEngine();
    void run();
    void test();
    void process_conjunctive_query(Query& query);
    void process_disjunctive_query(Query& query);
    
private:
    std::string data_path_;
    float average_doc_length_;
    std::map<std::string, WordInfo> words_;
    std::vector<DocInfo> docs_;
    int doc_num_;
    int top_k_;
    bool show_running_time_;

    // in bm25 score, the left part is a function of query words,
    // it's same for different doc, so we can cache this part
    // this make the disjunctive query "new york university" score
    // computing time 3.79s->2.13s, total time: 5.65s->3.57s (200m doc)
    std::map<int, float> bm25_score_left_cache_;
    float K_[K_CACHE_SIZE];

    std::ifstream new_text_file_;
    
    std::ifstream inverted_index_file_;

    void read_in_words_and_urls();

    bool open_list(std::string& word, PostingList& posting_list);
    int next_geq(PostingList& posting_list, int k);
    int next_geq_linear(PostingList& posting_list, int k);
    int get_freq(PostingList& posting_list);

    Query get_query();
    void preprocess_query(Query& query);
    QueryRes generate_query_res(std::vector<PostingList>& posting_list_vec,
            Query& query, int doc_id, std::vector<bool>& is_valid);
    void insert_into_top_k(std::vector<QueryRes>& resutls, QueryRes& res);

    void display_results(Query& query, std::vector<QueryRes>& results);
};
