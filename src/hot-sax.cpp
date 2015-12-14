#include <RcppArmadillo.h>
using namespace Rcpp ;
//
#include <jmotif.h>
//

discord_record find_best_discord_hot_sax(NumericVector ts, int w_size,
          std::map<std::string, std::vector<int> > &word2indexes,
          std::multimap<int, std::string> &ordered_words, VisitRegistry* globalRegistry) {

  // searching for the discord
  //
  double best_so_far_distance = 0;
  int best_so_far_index = -1;
  CharacterVector best_so_far_word = "";

  VisitRegistry outerRegistry(ts.size() - w_size);

  // outer heuristics ver the magic array
  for(std::multimap<int, std::string>::iterator it = ordered_words.begin();
      it != ordered_words.end(); ++it) {

    // Rcout << " examining " << it->second << " seen " << it->first << " times\n";
    // current word occurences
    std::vector<int> word_occurrences = word2indexes[it->second];
    for(unsigned i=0; i<word_occurrences.size(); i++){

      int candidate_idx = word_occurrences[i];
      if(globalRegistry->isVisited(candidate_idx)){
        continue;
      }
      NumericVector candidate_seq = subseries(ts, candidate_idx, candidate_idx + w_size);

      VisitRegistry innerRegistry(ts.size() - w_size);
      bool doRandomSearch = true;
      double nnDistance = std::numeric_limits<double>::max();

      // short loop over the similar sequencing finding the best distance
      for(unsigned j=0; j<word_occurrences.size(); j++){

        int inner_idx = word_occurrences[j];
        innerRegistry.markVisited(inner_idx);

        // Rcout << innerRegistry.unvisited_count << ", " << inner_idx << "\n";
        if( std::abs(inner_idx-candidate_idx) > w_size){
          NumericVector curr_seq = subseries(ts, inner_idx, inner_idx + w_size);
          double dist = euclidean_dist(candidate_seq, curr_seq);
          if(dist < nnDistance){
            nnDistance = dist;
          }
          if(dist < best_so_far_distance){
            doRandomSearch = false;
            //Rcout << "  abandoning early search... \n";
            break;
          }
        }
      }
      // Rcout << " same word iterations finished with nnDistance " << nnDistance <<
      //  ", best so far distance " << best_so_far_distance << "\n";

      if(doRandomSearch){
        //Rcout << " doing random search... \n";

        int inner_idx = innerRegistry.getNextUnvisited();

        while(!(-1==inner_idx)){
          innerRegistry.markVisited(inner_idx);
          //Rcout << innerRegistry.unvisited_count << ", " << inner_idx << "\n";

          if( std::abs(inner_idx-candidate_idx) > w_size){
            NumericVector curr_seq = subseries(ts, inner_idx, inner_idx + w_size);
            double dist = euclidean_dist(candidate_seq, curr_seq);
            if(dist < nnDistance){
              nnDistance = dist;
            }
            if(dist < best_so_far_distance){
              //Rcout << "  abandoning random search... \n";
              break;
            }
          }
          inner_idx = innerRegistry.getNextUnvisited();
        }
      }
      //Rcout << " ended random iterations\n";

      if(nnDistance > best_so_far_distance && nnDistance < std::numeric_limits<double>::max()){
        best_so_far_distance = nnDistance;
        best_so_far_index = candidate_idx;
        best_so_far_word = it->second;
        //Rcout << "updated discord record: "<< best_so_far_word << " at " << best_so_far_index <<
        //  " nnDistance " << best_so_far_distance << "\n";
      }

      //Rcout << "discord: "<< best_so_far_word << " at " << best_so_far_index <<
      //  " nnDistance " << best_so_far_distance << "\n";
    }

  }

  struct discord_record res;
  res.index = best_so_far_index;
  res.nn_distance = best_so_far_distance;
  return res;
}

//' Finds a discord with HOT-SAX.
//'
//' @param ts the input timeseries.
//' @param w_size the sliding window size.
//' @param paa_size the PAA size.
//' @param a_size the alphabet size.
//' @param n_threshold the normalization threshold.
//' @param discords_num the number of discords to report.
//' @useDynLib jmotif
//' @export
//' @references Keogh, E., Lin, J., Fu, A.,
//' HOT SAX: Efficiently finding the most unusual time series subsequence.
//' Proceeding ICDM '05 Proceedings of the Fifth IEEE International Conference on Data Mining
//' @examples
//' discords = find_discords_hot_sax(ecg0606, 100, 4, 4, 0.01, 1)
//' plot(ecg0606, type = "l", col = "cornflowerblue", main = "ECG 0606")
//' lines(x=c(discords[1,2]:(discords[1,2]+100)),
//'    y=ecg0606[discords[1,2]:(discords[1,2]+100)], col="red")
// [[Rcpp::export]]
Rcpp::DataFrame find_discords_hot_sax(NumericVector ts, int w_size, int paa_size,
                                      int a_size, double n_threshold, int discords_num) {

  // first step - fill in these maps which are the direct and inverse indices
  //
  std::map<int, std::string> idx2word;
  std::map<std::string, std::vector<int> > word2indexes;

  CharacterVector old_str("");
  for (int i = 0; i <= ts.length() - w_size; i++) {

    NumericVector subSection = subseries(ts, i, i + w_size);
    subSection = znorm(subSection, n_threshold);
    subSection = paa(subSection, paa_size);
    CharacterVector curr_str = series_to_string(subSection, a_size);

    idx2word.insert(std::make_pair(i, Rcpp::as<std::string>(curr_str)));
    if (word2indexes.find(Rcpp::as<std::string>(curr_str)) == word2indexes.end()){
      std::vector<int> v; // since no entry has been found add the new one
      v.push_back(i);
      word2indexes.insert(std::make_pair(Rcpp::as<std::string>(curr_str), v));
    }else{
      word2indexes[Rcpp::as<std::string>(curr_str)].push_back(i); // add the idx to an existing entry
    }
    old_str = curr_str;
  }

  // this is a magic arry map that is ordered by the words frequency
  //
  std::multimap<int, std::string> ordered_words;
  for(std::map<std::string, std::vector<int> >::iterator it = word2indexes.begin();
      it != word2indexes.end(); ++it) {
    ordered_words.insert(std::make_pair( (it->second).size(), it->first));
  }

  std::map<int, double> res;

  VisitRegistry registry(ts.length());
  registry.markVisited(ts.length() - w_size, ts.length());

  // Rcout << "starting search of " << discords_num << " discords..." << "\n";

  int discord_counter = 0;
  while(discord_counter < discords_num){

    discord_record rec = find_best_discord_hot_sax(ts, w_size, word2indexes, ordered_words, &registry);

    if(rec.nn_distance == 0 || rec.index == -1){ break; }

    res.insert(std::make_pair(rec.index, rec.nn_distance));

    int start = rec.index - w_size;
    if(start<0){
      start = 0;
    }
    int end = rec.index + w_size;
    if(end>=ts.length()){
      end = ts.length();
    }

    // Rcout << "marking as visited from " << start << " to " << end << "\n";
    registry.markVisited(start, end);
    discord_counter = discord_counter + 1;
  }

  std::vector<int> positions;
  std::vector<double > distances;

  for(std::map<int, double>::iterator it = res.begin(); it != res.end(); it++) {
    positions.push_back(it->first);
    distances.push_back(it->second);
  }
  // make results
  return Rcpp::DataFrame::create(
    Named("nn_distance") = distances,
    Named("position") = positions
  );

}