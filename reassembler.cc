#include "reassembler.hh"

using namespace std;

Reassembler::Reassembler(const size_t capacity):_capacity(capacity){}


void Reassembler::insert_to_unassembled(const std::string&data,const size_t index,bool eof){
    if(unassembled.count(index)){
      auto&[str,_]=unassembled[index];
      if(data.size()>str.size())str=data,_=_||eof;
    }
    else unassembled[index]={data,eof};
}


void Reassembler::insert( uint64_t first_index, string data, bool is_last_substring, Writer& output )
{
  // Your code here.
  (void)first_index;
  (void)data;
  (void)is_last_substring;
  (void)output;
   first_unread=output.reader().bytes_popped();
   first_unacceptable=first_unread+_capacity;
  if(first_index+data.size()<=first_unacceptable){
    insert_to_unassembled(std::move(data),first_index,is_last_substring);
  }
  else insert_to_unassembled(std::move(data.substr(0,first_unacceptable-first_index)),first_index,is_last_substring);
  while(!unassembled.empty()){
     auto &[idx,pair]=*unassembled.begin();
     if(idx<=first_unassembled){
      if(idx+pair.first.size()>first_unassembled){
        output.push(pair.first.substr(first_unassembled-idx));
        first_unassembled=idx+pair.first.size();
      }
      if(pair.second)output.close();
       unassembled.erase(unassembled.begin());
     }
     else break;
  }
     vector<std::pair<size_t, std::pair<std::string, bool>>> vec;
    num_of_unassembled_bytes = 0;
    for(auto it = unassembled.begin(); it != unassembled.end(); ++it)
    {
        if(vec.empty() || it->first > vec.back().first + vec.back().second.first.size())
        {
            vec.push_back(*it);
            num_of_unassembled_bytes += it->second.first.size();
            continue;
        }
        auto &back = vec.back();
        auto &idx = back.first; auto &str = back.second.first;
        
        if(it->first + it->second.first.size() < idx + str.size())continue;
        num_of_unassembled_bytes += it->second.first.size() - idx - str.size() + it->first;
        str += it->second.first.substr(idx + str.size() - it->first);
    }
    unassembled = std::move(std::map<size_t, std::pair<std::string, bool>>(vec.begin(), vec.end()));
}

uint64_t Reassembler::bytes_pending() const
{
  // Your code here.
  return num_of_unassembled_bytes;
}
