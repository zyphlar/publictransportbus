/*
Copyright (c) 2001-2022, Hove
This is an example file, do whatever you want with it! (for example if you are in Paris, invite us for a beer)

This shows the simplest way to use the osm.pbf reader. It just counts the number of objects in the file.

To build this file :
g++ -O2 -o counter gettingosmdata.cc -losmpbf -lprotobuf

To run it:
./getdata isle-of-man-latest.osm.pbf stops.txt ./outfolder
*/

#include "libosmpbfreader/osmpbfreader.h"
#include <iostream>
#include <string>
#include <list>
#include <typeinfo>
#include <vector>
#include <fstream>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <stdexcept>
#include "readgt.h"
#include "vinceinv.h"

using namespace osmpbfreader;

struct osmcoord {
    uint64_t id;
    double lati;
    double loni;
};

// We need to define a visitor with three methods that will be called while the file is read
struct Counter {
    // Three integers count how many times each object type occurs
    int nodes;
    int ways;
    int relations;

     std::vector<osmcoord> vlop;

    Counter() : nodes(0), ways(0), relations(0) {}

    // This method is called every time a Node is read
    void node_callback(uint64_t osmid, double lon, double lat, const Tags &tags){

          auto highw = tags.find("highway");
          if(highw != tags.end() && highw->second=="bus_stop") {
            ++nodes;
          vlop.push_back({osmid, lat, lon});
            
            std::cout << "osmid" << osmid <<  "tags" << &tags <<  "lon" << lon << "lat" << lat << std::endl; //this line works
            
        }

    }

    // This method is called every time a Way is read
    // refs is a vector that contains the reference to the nodes that compose the way
    void way_callback(uint64_t /*osmid*/, const Tags &/*tags*/, const std::vector<uint64_t> &/*refs*/){
        ++ways;
    }

    // This method is called every time a Relation is read
    // refs is a vector of pair corresponding of the relation type (Node, Way, Relation) and the reference to the object
    void relation_callback(uint64_t /*osmid*/, const Tags &/*tags*/, const References & /*refs*/){
        ++relations;
    }
};

bool isDecimal(std::string const &str)
{
    auto it = str.begin();
    while (it != str.end() && (std::isdigit(*it) || std::ispunct(*it) ) ) {
        it++;
    }
    return !str.empty() && it == str.end();
}

int main(int argc, char** argv) {
    if(argc != 4) {
        std::cout << "Usage: " << argv[0] << " file_to_read.osm.pbf stops.txt /path/to/output/folder" << std::endl;
        return 1;
    }

    std::list<Stop> stops;
    readgtfs(stops,argv[2]);

    Counter counter;
    read_osm_pbf(argv[1], counter);
    //std::cout << "We read " << nodes << " counter.nodes " << counter.ways << " ways and " << counter.relations << " relations" << std::endl;
    std::cout << "number of nodes that fit" << counter.nodes << std::endl; //this line works
    std::cout << "ways info" << counter.ways << std::endl; //this line works

    int numprocessed = 0;
    int nummatch = 0;
    int numnomatch = 0;

    std::ofstream myfile;
    std::string outpath(argv[3]);
    outpath.append("/output.geojson");
    myfile.open(outpath);
    myfile << R"x({"type": "FeatureCollection","features": [)x";

    for (const auto& stop : stops) {
      double s;
      bool found = false;
      std::string stlow = stop.stop_lon;
      std::string stlaw = stop.stop_lat;
      numprocessed++;

      for (const auto& coord : counter.vlop) {
        s = vincentydistt(stlow, stlaw, coord.loni, coord.lati);

        // once we find a match, we're done searching OSM for this stop
        if (s < 30) {
          myfile << R"x({"type": "Feature","geometry": {"type": "Point","coordinates": [)x";
          myfile << coord.loni << ", " << coord.lati;
          myfile << R"x(]},"properties":{"name":"OSM","color":"green","id":")x";
          myfile << coord.id;
          myfile << R"x("}},)x";
          myfile << R"x({"type": "Feature","geometry": {"type": "Point","coordinates": [)x";
          myfile << stop.stop_lon << ", " << stop.stop_lat;
          myfile << R"x(]},"properties":{"name":"GTFS","color":"lime","id":")x";
          myfile << stop.stop_id;
          myfile << R"x("}},)x";
          myfile << "\n";

          nummatch++;
          std::cout << "for less than 30m, distance away" << s  << std::endl;
          std::cout << "osm lat: " << coord.lati << "osm lon: " << coord.loni << std::endl;
          std::cout << "gtfs lat: " << stop.stop_lat << "gtfs lon: " << stop.stop_lon << std::endl;
          found = true;
          break; 
        }
      } 

      // if we searched all of OSM and found nothing for this GTFS stop, output
      // (note the first entry is the CSV header, and some coordinates may be non-numeric [invalid])
      if (!found && isDecimal(stop.stop_lon) && isDecimal(stop.stop_lat)) {
        myfile << R"x({"type": "Feature","geometry": {"type": "Point","coordinates": [)x";
        myfile << stop.stop_lon << ", " << stop.stop_lat;
        myfile << R"x(]},"properties":{"name":"GTFS","color":"red","id":")x";
        myfile << stop.stop_id;
        myfile << R"x("}},)x";

        numnomatch++;
      }
    }

    // seek backwards by one char to delete the last character (an invalid trailing comma)
    long pos = myfile.tellp();
    myfile.seekp(pos-1);
    myfile << "] }";
    myfile.close();

    printf("\nResult: %d matched, %d no match, %d total\n", nummatch, numnomatch, numprocessed);
  
    return 0;
}
