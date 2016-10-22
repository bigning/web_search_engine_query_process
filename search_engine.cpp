#include "search_engine.h"
#include <fstream>

SearchEngine::SearchEngine() {
    data_path_ = "../data/postings_from_doc/";
}

void SearchEngine::run() {
    inverted_index_file_.open((data_path_ + "inverted_list").c_str(),
            std::ifstream::binary);
    read_in_words_and_urls();

    test();
}

void SearchEngine::test() {
    PostingList posting_list;
    std::string word = "additives";
    if (open_list(word, posting_list)) {
        //next_geq(posting_list, 10);
    } else {
        std::cout << "no posting for " << word << std::endl;
    }
}

void SearchEngine::read_in_words_and_urls() {
    float total_doc_length = 0;

    std::cout << "[INIT]: loading words and urls" << std::endl;
    std::string words_info_filename = data_path_ + "word_to_list";
    std::ifstream words_info_file(words_info_filename.c_str());

    std::string word;
    long long pos = 0;
    int num = 0;
    std::string previous_word;
    while (words_info_file >> word >> pos >> num) {
        WordInfo word_info;
        word_info.word = word;
        word_info.start_position = pos;
        word_info.end_position = 0;
        word_info.num_of_doc_contain_this_word = num;

        words_.insert(std::pair<std::string, WordInfo>(word, word_info));
        if (previous_word.size() > 0) {
            words_[previous_word].end_position = pos;
        }
        previous_word = word;
    }
    // last word, end position is -1;
    inverted_index_file_.seekg(0, std::ios::end);
    words_[previous_word].end_position = (long long)(inverted_index_file_.tellg())
        + 1;

    words_info_file.close();

    std::string docs_filename = data_path_ + "urltable";
    std::ifstream docs_file(docs_filename.c_str());
    int doc_id = 0;
    int length = 0;
    std::string url, data_file_name;
    while (docs_file >> doc_id >> length >> url >> data_file_name >> pos) {
        DocInfo doc_info(doc_id, url, length, pos);
        docs_.push_back(doc_info);
        total_doc_length += length;
    }

    docs_file.close();

    average_doc_length_ = total_doc_length / (float)(docs_.size());
    doc_num_ = docs_.size();
}

bool SearchEngine::open_list(std::string& word, PostingList& posting_list) {
    std::map<std::string, WordInfo>::iterator iter =
        words_.find(word);
    if (iter == words_.end()) {
        return false;
    }
    posting_list = PostingList(iter->second.start_position, 
            iter->second.end_position, iter->second.start_position);
    return true;
}

/*
 * find the next posting in list posting_list with doc_id>=k,
 * return corresponding posting_info with such doc_id,
 * if none exists, return false
*/
bool SearchEngine::next_geq(PostingList& posting_list, int k,
        PostingInfo& posting_info) {
    if (posting_list.current >= posting_list.end) return false;

    get_current_posting_info(posting_list.current, posting_info);
    if (posting_info.doc_id >= k) return true;

    while (posting_info.doc_id < k && posting_list.current < posting_list.end) {
        get_current_posting_info(posting_list.current, posting_info);
    }

    if (posting_info.doc_id >= k) return true;
    return false;
}

void SearchEngine::get_current_posting_info(long long& current_pos,
        PostingInfo& posting_info) {
    inverted_index_file_.seekg(current_pos);
    int doc_id = 0;
    int freq = 0;
    int pos = 0;
    inverted_index_file_.read((char*)(&doc_id), sizeof(doc_id));
    inverted_index_file_.read((char*)(&freq), sizeof(freq));
    std::vector<int> positions;
    for (int i = 0; i < freq; i++) {
        inverted_index_file_.read((char*)(&pos), sizeof(pos));
        positions.push_back(pos);
    }
    posting_info.doc_id = doc_id;
    posting_info.freq = freq;
    posting_info.pos = positions;
    current_pos += (2 + positions.size()) * sizeof(int);
}

int main() {
    SearchEngine* p_engine = new SearchEngine();
    p_engine->run();
    std::cout << "hello world" << std::endl;
    return 0;
}
