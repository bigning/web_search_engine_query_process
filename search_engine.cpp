#include "color.h"
#include "search_engine.h"
#include <fstream>
#include <stdio.h>
#include <iostream>
#include <sstream>
#include <math.h>
#include <algorithm>
#include <iterator>
#include <set>
#include <ctime>

SearchEngine::SearchEngine() {
    data_path_ = "../data/postings_from_doc/";
    top_k_ = 20;
    new_text_file_.open((data_path_ + "new_text").c_str(), std::ios::binary);
    show_running_time_ = true;
}

void SearchEngine::run() {
    inverted_index_file_.open((data_path_ + "inverted_list").c_str(),
            std::ifstream::binary);
    std::clock_t begin = clock();
    read_in_words_and_urls();
    std::clock_t end = clock();
    if (show_running_time_) {
        std::cout << "[TIME]: " << double(end - begin) / CLOCKS_PER_SEC << std::endl;;
    }

    while (true) {
        Query query = get_query();
        preprocess_query(query);
        if (query.is_conjunctive) {
            process_conjunctive_query(query);
        } else {
            process_disjunctive_query(query);
        }
    }
}

/* 
 * remove query words those are not in words_
*/
void SearchEngine::preprocess_query(Query& query) {
    std::vector<std::string> words;
    for (int i = 0; i < query.words.size(); i++) {
        if (words_.find(query.words[i]) == words_.end()) {
            std::cout << "[WARNING]: remove word " << query.words[i]
                << " from query, because it never appears in all docs" 
                << std::endl;
            continue;
        }
        words.push_back(query.words[i]);
    }
    query.words = words;
    bm25_score_left_cache_.clear();
}

void SearchEngine::process_conjunctive_query(Query& query) {
    if (query.words.size() == 0) {
        std::cout << "[WARNING]: no valid query word" << std::endl;
        return;
    }
    
    std::clock_t begin = clock();
    std::vector<PostingList> posting_list_vec(query.words.size());
    for (int i = 0; i < query.words.size(); i++) {
        if (!open_list(query.words[i], posting_list_vec[i])) {
            std::cout << "[ERROR]: can't open posting list for word: "
                << query.words[i] << std::endl;
        }
    }
    std::clock_t end = clock();
    if (show_running_time_) {
        std::cout << "[TIME]: loading postings " <<
            double(end - begin) / (double)(CLOCKS_PER_SEC) << std::endl;;
    }

    begin = clock();
    std::vector<QueryRes> query_results;
    int did = 0;
    int d = 0;
    std::vector<bool> is_valid(posting_list_vec.size(), true);
    std::clock_t begin_nest, end_nest;
    double time_next_geq = 0, time_generate_query_res = 0, time_insert= 0;
    while (did < doc_num_) {
        begin_nest = clock();
        did = next_geq(posting_list_vec[0], did);
        if (did >= doc_num_) break;
        for (int i = 1; i < query.words.size(); i++) { 
                //(d = next_geq(posting_list_vec[i], did)) == did; i++) {
            d = next_geq(posting_list_vec[i], did);
            if (d != did) break;
        }
        end_nest = clock();
        time_next_geq += double(end_nest - begin_nest) / double(CLOCKS_PER_SEC);
        if (d > did) { 
            did = d; /* not in intersection */
            if (did >= doc_num_) break;
        } else {
            begin_nest = clock();
            QueryRes query_res = generate_query_res(posting_list_vec, query, did,
                    is_valid);
            end_nest = clock();
            time_generate_query_res += double(end_nest - begin_nest) / double(CLOCKS_PER_SEC);

            begin_nest = clock();
            insert_into_top_k(query_results, query_res);
            end_nest = clock();
            time_insert += double(end_nest - begin_nest) / double(CLOCKS_PER_SEC);
            did++;
        }
    }
    //std::cout << query_results.size() << std::endl;
    end = clock();

    display_results(query, query_results);
    std::cout << std::endl;

    if (show_running_time_) {
        std::cout << "[TIME]: get conjunctive results " <<
            double(end - begin) / (double)(CLOCKS_PER_SEC) << std::endl;;

        std::cout << "[TIME]: next_geq " << time_next_geq << std::endl;
        std::cout << "[TIME]: generate_query_res " << time_next_geq << std::endl;
        std::cout << "[TIME]: insertion " << time_insert << std::endl;
    }
}

void SearchEngine::process_disjunctive_query(Query& query) {
    if (query.words.size() == 0) {
        std::cout << "[WARNING]: no valid query word" << std::endl;
        return;
    }
    std::vector<PostingList> posting_list_vec(query.words.size());
    for (int i = 0; i < query.words.size(); i++) {
        if (!open_list(query.words[i], posting_list_vec[i])) {
            std::cout << "[ERROR]: can't open posting list for word: "
                << query.words[i] << std::endl;
        }
    }
    int posting_list_vec_size = posting_list_vec.size();
    std::vector<QueryRes> query_results;

    std::clock_t begin_nest, end_nest;
    double time_next_geq = 0, time_generate_query_res = 0, time_insert= 0, time_find_min = 0;
    std::clock_t begin = clock();

    int current_mimimum = doc_num_;
    for (int i = 0; i < query.words.size(); i++) {
        if (posting_list_vec[i].postings[0].doc_id < current_mimimum) {
            current_mimimum = posting_list_vec[i].postings[0].doc_id;
        }
    }
    int next_mimimum = doc_num_;
    int process_num = 0;
    while (current_mimimum < doc_num_) {
        int valid_num = 0;
        std::vector<bool> is_valid(posting_list_vec_size, false);
        int doc_id = 0;
        begin_nest = clock();
        for (int i = 0; i < posting_list_vec_size; i++) {
            doc_id = next_geq_linear(posting_list_vec[i], current_mimimum);
            if (doc_id == current_mimimum) {
                valid_num++;
                //is_valid.push_back(true);
                is_valid[i] = true;
                if (posting_list_vec[i].current + 1 < posting_list_vec[i].postings.size() &&
                        posting_list_vec[i].postings[posting_list_vec[i].current + 1].doc_id <
                        next_mimimum) {
                    next_mimimum =
                        posting_list_vec[i].postings[posting_list_vec[i].current + 1].doc_id;
                }
            } else {
                if (doc_id < next_mimimum) {
                    next_mimimum = doc_id;
                }
                //is_valid.push_back(false);
            }
        }
        end_nest = clock();
        time_next_geq += (double)(end_nest - begin_nest) / (double)CLOCKS_PER_SEC;

        if (valid_num > 0) {
            begin_nest = clock();
            QueryRes query_res = generate_query_res(posting_list_vec, 
                    query, current_mimimum, is_valid);
            end_nest = clock();
            time_generate_query_res += (double)(end_nest - begin_nest) / (double)CLOCKS_PER_SEC;

            begin_nest = clock();
            insert_into_top_k(query_results, query_res);
            end_nest = clock();
            time_insert += (double)(end_nest - begin_nest) / (double)CLOCKS_PER_SEC;
        }

        current_mimimum = next_mimimum;
        next_mimimum = doc_num_;
        process_num++;
        /*
        if (process_num % 10000 == 0 && show_running_time_) {
            std::cout << "[TIME]: next_get " << time_next_geq << std::endl;
            std::cout << "[TIME]: insert " << time_insert << std::endl;
            std::cout << "[TIME]: generate_query_res " << time_generate_query_res << std::endl;
        }
        */
    }
    std::clock_t end = clock();

    std::cout << "results: " << query_results.size() << std::endl;
    display_results(query, query_results);
    std::cout << std::endl;

    if (show_running_time_) {
        std::cout << "[TIME]: disjunctive process " 
            << (double)(end - begin) / (double)CLOCKS_PER_SEC << std::endl;
        std::cout << "[TIME]: next_get " << time_next_geq << std::endl;
        std::cout << "[TIME]: insert " << time_insert << std::endl;
        std::cout << "[TIME]: generate_query_res " << time_generate_query_res << std::endl;
        std::cout << "[TIME]: find_min " << time_find_min << std::endl;
    }
}

void SearchEngine::display_results(Query& query, std::vector<QueryRes>& results) {
    for (int i = 0; i < results.size(); i++) {
        std::cout << std::endl << std::endl << BLUE << i << YELLOW << "\t[doc_id]: " << RESET << results[i].doc_id << YELLOW << ".\t[score]: "
            << RESET << results[i].score << YELLOW << "\t[url]: " << RESET << docs_[results[i].doc_id].url
            << std::endl;;
        std::cout << "\t";
        for (int k = 0; k < results[i].pos.size(); k++) {
            std::cout << YELLOW << query.words[k] << ": " << RESET
                << results[i].pos[k].size() << " ";
        }
        std::cout << std::endl;

        long long start = docs_[results[i].doc_id].start_position;
        long long end;
        if (results[i].doc_id != doc_num_ - 1) {
            end = docs_[results[i].doc_id + 1].start_position;
        } else {
            new_text_file_.seekg(0, std::ios::end);
            end = (long long)(new_text_file_.tellg()) + 1;
        }
        char* text_buf = new char[end - start];
        new_text_file_.seekg(start);
        new_text_file_.read(text_buf, end - start);
        std::string text(text_buf);
        
        std::istringstream iss(text);
        std::vector<std::string> words;
        std::copy(std::istream_iterator<std::string>(iss), 
                std::istream_iterator<std::string>(),
                std::back_inserter(words));
        std::string output;

        std::set<std::string> query_words_set;
        for (int k = 0; k < query.words.size(); k++) {
            query_words_set.insert(query.words[k]);
        }

        std::cout << "\t";
        for (int k = 0; k < results[i].pos.size(); k++) {
            if (results[i].pos[k].size() == 0) continue;
            //for (int j = 0; j < results[i].pos[k].size(); j++) {
            if (results[i].pos[k].size() >= 1) {
                for (int j = 0; j < 1; j++) {
                    int start_ind = results[i].pos[k][j] - 5 > 0 ? 
                        results[i].pos[k][j] - 5 : 0;
                    int end_ind = results[i].pos[k][j] + 5 < words.size() ? 
                        results[i].pos[k][j] + 5 : words.size();
                    std::cout << "...";
                    for (int m = start_ind; m < end_ind; m++) {
                        //if (words[m] == query.words[k]) {
                        if (query_words_set.find(words[m]) != query_words_set.end()) {
                            std::cout << RED << words[m] << RESET << " ";
                        } else {
                            std::cout << words[m] << " ";
                        }
                    }
                    std::cout << "... ";
                }
            }
        }

        delete[] text_buf;
    }
    std::cout << std::endl << std::endl;
}

void SearchEngine::insert_into_top_k(std::vector<QueryRes>& results,
        QueryRes& res) {
    if (results.size() == 0) {
        results.push_back(res);
        return;
    }
  
    int results_size = results.size();
    if (results_size >= top_k_ && res.score < results[results_size - 1].score) {
        return;
    }

    bool inserted = false;
    std::vector<QueryRes>::iterator iter = results.begin();
    for (; iter != results.end(); iter++) {
        if (iter->score < res.score) {
            results.insert(iter, res);
            inserted = true;
            break;
        }
    }
    if (!inserted) {
        results.insert(results.end(), res);
    }
    if (results_size >= top_k_) {
        results.erase(results.end());
    }
}

QueryRes SearchEngine::generate_query_res(
        std::vector<PostingList>& posting_list_vec,
        Query& query, int doc_id, std::vector<bool>& is_valid) {
    QueryRes res;
    res.doc_id = doc_id;
    res.pos = std::vector<std::vector<int> >(query.words.size());
    for (int i = 0; i < posting_list_vec.size(); i++) {
        if (!is_valid[i]) continue;
        res.pos[i] = posting_list_vec[i].postings[posting_list_vec[i].current].pos;
    }

    float k1 = 1.2;
    float b = 0.75;
    float N = (float)doc_num_;
    float score = 0;
    int d = (docs_[doc_id].length);
    std::map<int, float>::iterator iter;
    float K = 0;
    if (d < K_CACHE_SIZE) {
        K = K_[d];
    } else {
        K = k1 * ((1 - b) + b * (float)d / average_doc_length_);
    }
    for (int i = 0; i < query.words.size(); i++) {
        if (!is_valid[i]) {
            continue;
        }
        float left = 0;

        iter = bm25_score_left_cache_.find(i);
        if (iter == bm25_score_left_cache_.end()) {
            float ft = (float)(words_[query.words[i]].num_of_doc_contain_this_word);
            left = log((N - ft + 0.5) / (ft + 0.5));
            bm25_score_left_cache_.insert(std::pair<int, float>(i, left));
        } else {
            left = bm25_score_left_cache_[i];
        }

        float fdt = (float)(res.pos[i].size());
        float right_up = (k1 + 1) * fdt;
        float right_down = K + fdt;
        float right = right_up / right_down;

        score += left * right;
    }
    res.score = score;
    return res;
}

void SearchEngine::test() {
    PostingList posting_list;
    std::string word = "additives";
    if (open_list(word, posting_list)) {
        std::cout << posting_list.postings.size() << std::endl 
            << posting_list.postings[0].doc_id << " " << posting_list.postings[0].freq << " " << posting_list.postings[0].pos[0] << std::endl;
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
        /*
        WordInfo word_info;
        word_info.word = word;
        word_info.start_position = pos;
        word_info.end_position = 0;
        word_info.num_of_doc_contain_this_word = num;
        */

        words_.insert(std::pair<std::string, WordInfo>(word, WordInfo(word, pos, 0, num)));
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

    int max_doc_num = 10000000;
    int real_doc_num = 0;
    DocInfo doc_info_tmp;
    docs_.resize(max_doc_num, doc_info_tmp);
    std::string docs_filename = data_path_ + "urltable";
    std::ifstream docs_file(docs_filename.c_str());
    int doc_id = 0;
    int length = 0;
    std::string url, data_file_name;
    while (docs_file >> doc_id >> length >> url >> data_file_name >> pos) {
        //DocInfo doc_info(doc_id, url, length, pos);
        //docs_.push_back(doc_info);
        docs_[real_doc_num++] = DocInfo(doc_id, url, length, pos);
        total_doc_length += length;
    }

    docs_file.close();
    docs_.resize(real_doc_num);

    average_doc_length_ = total_doc_length / (float)(real_doc_num);
    doc_num_ = real_doc_num;

    // fill K_
    float k1 = 1.2, b = 0.75;
    for (int i = 0; i < K_CACHE_SIZE; i++) {
        K_[i] = k1 * ((1 - b) + b * (float)i / average_doc_length_);
    }
}

bool SearchEngine::open_list(std::string& word, 
        PostingList& posting_list) {
    std::map<std::string, WordInfo>::iterator iter =
        words_.find(word);
    if (iter == words_.end()) {
        return false;
    }

    //posting_list.postings.clear();
    long long start = iter->second.start_position;
    long long end = iter->second.end_position;
    int int_num = (end - start) / sizeof(int);
    int* int_buffer = new int[int_num];

    // read postings to memory
    inverted_index_file_.seekg(start);
    inverted_index_file_.read((char*)int_buffer, end - start);

    int max_posting_num = int_num/3;
    int real_posting_num = 0;

    int doc_id = 0, freq = 0;
    int index = 0;
    std::clock_t begin = clock();
    PostingInfo post;
    posting_list.postings.resize(max_posting_num, post);
    //posting_list.postings = std::vector<PostingInfo>(max_posting_num, PostingInfo());
    while (index < int_num) {
        doc_id = int_buffer[index++];
        freq = int_buffer[index++];
        std::vector<int> positions(freq);
        for (int i = 0; i < freq; i++) {
            positions[i] = int_buffer[index++];
        }
        //PostingInfo posting_info(doc_id, freq, positions);
        //posting_list.postings.push_back(posting_info);
        posting_list.postings[real_posting_num++] = PostingInfo(doc_id, freq, positions);
    }
    posting_list.postings.resize(real_posting_num);
    std::clock_t end_t = clock();
    posting_list.current = 0;
    //std::cout << "[time]: " << (double)(end_t - begin) / (double)CLOCKS_PER_SEC << std::endl;
    
    delete[] int_buffer;
    return true;
}

int SearchEngine::next_geq_linear(PostingList& posting_list, int k) {
    //if (posting_list.current < 0) posting_list.current = 0;
    if (posting_list.current >= posting_list.postings.size()) return doc_num_;
    if (posting_list.postings[posting_list.current].doc_id >= k) {
        return posting_list.postings[posting_list.current].doc_id;
    }

    // linear search
    while (posting_list.current < posting_list.postings.size() &&
        posting_list.postings[posting_list.current].doc_id < k) {
        posting_list.current++;
    }

    if (posting_list.current >= posting_list.postings.size()) return doc_num_;

    return posting_list.postings[posting_list.current].doc_id;
}

/*
 * find the next posting in list posting_list with doc_id>=k,
 * return doc_id,
 * if none exists, doc_num_ 
*/
int SearchEngine::next_geq(PostingList& posting_list, int k) {
    if (posting_list.current < 0) posting_list.current = 0;
    if (posting_list.current >= posting_list.postings.size()) return doc_num_;
    if (posting_list.postings[posting_list.current].doc_id >= k) {
        return posting_list.postings[posting_list.current].doc_id;
    }

    // binary search
    int left = posting_list.current;
    int right = posting_list.postings.size() - 1;
    int middle = (left + right) / 2;
    if (posting_list.postings[right].doc_id == k) {
        posting_list.current = right;
        return k;
    }
    if (posting_list.postings[right].doc_id < k) {
        posting_list.current = right;
        return doc_num_;
    }
    while (posting_list.postings[middle].doc_id != k && right - left >= 2) {
        if (posting_list.postings[middle].doc_id > k) {
            right = middle;
            middle = (left + right) / 2;
        } else {
            left = middle;
            middle = (left + right) / 2;
        }
    }
    if (posting_list.postings[middle].doc_id == k) {
        posting_list.current = middle;
        return posting_list.postings[posting_list.current].doc_id;
    }
    if (posting_list.postings[left].doc_id >= k) {
        posting_list.current = left;
        return posting_list.postings[posting_list.current].doc_id;
    } else {
        posting_list.current = right;
        return posting_list.postings[posting_list.current].doc_id;
    }
    // binary search
}

int SearchEngine::get_freq(PostingList& posting_list) {
    if (posting_list.current >= posting_list.postings.size()) {
        std::cout << 
            "[ERROR]: posting_list.current >= posting_list.postings.size()" 
            << std::endl;
        return -1;
    }
    return posting_list.postings[posting_list.current].freq;
}

Query SearchEngine::get_query() {
    char c;
    do {
        std::cout << "0 for disjunctive query, 1 for conjunctive query: " << std::endl;
        scanf("%c", &c);
    } while (c != '0' && c != '1');
    bool is_conjunctive;
    if (c == '0') {
        is_conjunctive = false;
    } else {
        is_conjunctive = true;
    }
    std::vector<std::string> words;
    std::string tmp;
    std::getline(std::cin, tmp);
    std::cout << "please input query words: " << std::endl;
    std::getline(std::cin, tmp);
    std::istringstream iss(tmp);
    std::string word;
    while (iss >> word) {
        words.push_back(word);
    }
    std::cout << std::endl << "[INFO]: "; 
    if (is_conjunctive) {
        std::cout << "CONJUNCTIVE QUERY: ";
    } else {
        std::cout << "DISJUNCTIVE QUERY: ";
    }
    for (int i = 0; i < words.size(); i++) {
        std::cout << words[i] << " ";
    }
    std::cout << std::endl;
    
    return Query(is_conjunctive, words);
}

int main() {
    SearchEngine* p_engine = new SearchEngine();
    p_engine->run();
    std::cout << "hello world" << std::endl;
    return 0;
}
