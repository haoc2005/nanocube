#include <iostream>
#include <random>
#include <chrono>
#include <sstream>
#include <iomanip>

#include <set>
#include <stdio.h>
#include <string.h>

#include <cmath>
#include <cstdlib>
#include <ctime>
#include <cstdio>

#include <unordered_map>

#include <string>
#include <fstream>
#include <sstream>

#include <datatiles/Stopwatch.hh>
#include <datatiles/TimeSeries.hh>
#include <datatiles/QuadTree.hh>
#include <datatiles/QuadTreeUtil.hh>
#include <datatiles/MercatorProjection.hh>

#include <datatiles/STree.hh>
#include <datatiles/FlatTree.hh>

// boost::mpl Metaprogramming Template
#include <boost/mpl/reverse.hpp>
#include <boost/mpl/vector.hpp>
#include <boost/mpl/size.hpp>
#include <boost/mpl/begin_end.hpp>
#include <boost/mpl/back.hpp>
#include <boost/mpl/front.hpp>
#include <boost/type_traits/is_same.hpp>
// #include <boost/mpl/next_prior.hpp>
// #include <boost/mpl/for_each.hpp>
// #include <boost/mpl/range_c.hpp>

using namespace std;

typedef uint32_t Int;

typedef float RealCoordinate;

using quadtree::Coordinate;
using quadtree::Count;
using quadtree::BitSize;
using quadtree::QuadTree;
using quadtree::Address;
using quadtree::Node;
using quadtree::Stats; // from QuadTreeUtil

using timeseries::TimeSeries;
using timeseries::TimeSeriesStatistics;

//
// Timestamp
//

typedef uint64_t Timestamp;

Timestamp mkTimestamp(string st, string format="%Y-%m-%d %H:%M:%S")
{
    struct tm tm;
    memset(&tm, 0, sizeof(struct tm));
    strptime(st.c_str(), format.c_str(), &tm);
    return (Timestamp) mktime(&tm);
}

Timestamp mkTimestamp(int year, int month, int day, int hour, int min, int sec)
{
    struct tm timeinfo;
    memset(&timeinfo, 0, sizeof(struct tm));
    timeinfo.tm_year = year	 - 1900;
    timeinfo.tm_mon  = month - 1;
    timeinfo.tm_mday = day;
    timeinfo.tm_hour = hour;
    timeinfo.tm_min  = min;
    timeinfo.tm_sec  = sec;

    /* call mktime: timeinfo->tm_wday will be set */
    Timestamp	timestamp = (Timestamp) mktime ( &timeinfo );

    //
    return timestamp;
}

//
// Tokenizer: copied from stack overflow solution from Evan Teran
//

std::vector<std::string> &split(const std::string &s, char delim, std::vector<std::string> &elems)
{
    std::stringstream ss(s);
    std::string item;
    while(std::getline(ss, item, delim)) {
        elems.push_back(item);
    }
    return elems;
}

//
// Auxiliar Text Formatting stuff
//

std::string fl(std::string st, int n=14)
{
    int l = st.size();
    if (l > n)
        return st;
    return st + std::string(n-l,' ');
}

std::string fr(std::string st, int n=14)
{
    int l = st.size();
    if (l > n)
        return st;
    return std::string(n-l,' ') + st;
}

template <class T>
inline std::string str(const T& t)
{
    std::stringstream ss;
    ss << std::setprecision(1) << std::fixed << t ;
    return ss.str();
}

template<typename Structure>
std::ostream& operator<<(std::ostream &os, const typename Structure::AddressType& addr)
{
    os << "Addr[x: "  << addr.x
       << ", y: "     << addr.y
       << ", level: " << addr.level
       << "]";
    return os;
}

class NullContent
{};

struct Point
{
    float    latitude;
    float    longitude;
    uint64_t timestamp;
};

//
// Small allocation TimeSeries
//


Count b2mb(Count b)
{
    Count mb = (Count) (b / (double) (1<<20));
    return mb;
}

//
// main
//

int main(int argc, char** argv)
{

    typedef uint32_t EventCount;
    typedef uint32_t EventTime;
    typedef TimeSeries<EventTime, EventCount> EventTimeSeries;

    const BitSize LevelBits = 25;
    typedef quadtree::QuadTree<LevelBits, EventTimeSeries> QuadTreeTS;

    // nested trees for STree
    typedef mpl::vector<QuadTreeTS> STreeTypes;

    // STree for TimeSeries
    typedef stree::STree<STreeTypes, EventTime> STreeTS;

    //
    STreeTS    stree;
    QuadTreeTS &hitS = stree.root;

    //
    typedef typename QuadTreeTS::AddressType QAddr;

    //
    Timestamp timeBinSizeInHours = 24;
    if (argc > 1)
        timeBinSizeInHours = atoi(argv[1]);
    if (timeBinSizeInHours == 0)
        throw std::string("Invalid Time Bin Size");

    //
    // dumpTypeSizes(std::clog);

    std::clog << "Time Bin Size in Hours: " << timeBinSizeInHours << std::endl;

    //
    Stopwatch    stopwatch;
    // Milliseconds timeAddAll;

    // show header of report
    char sep = '|';
    std::clog
        << sep
        << fl("Add Calls")     << sep // number of add calls
        << fl("Num Nodes NN")  << sep // num nodes in quad tree
        << fl("Num Leaves NL") << sep //
        << fl("NN/NL")         << sep //
        << fl("QT Size (MB)")     << sep
        << fl("TS Size (MB)")     << sep
        << fl("Total (MB)")       << sep
        << fl("Time Add (ms)")      << sep << std::endl;


    auto showProgress = [&] ()
        {
            // report
            std::clog
            << sep
            << fr(str(hitS.getNumAdds()))     << sep // number of add calls
            << fr(str(hitS.getNumNodes()))    << sep // num nodes in quad tree
            << fr(str(hitS.getNumLeaves()))   << sep // num nodes in quad tree
            << fr(str((double)hitS.getNumNodes()/(double)hitS.getNumLeaves())) << sep // num nodes in quad tree
            << fr(str(b2mb(hitS.getMemoryUsage()))) << sep // num nodes in quad tree
            << 0 << sep // fr(str(b2mb(timeSeriesAddPolicy.memUsage))) << sep // num nodes in quad tree
            << 0 << sep // fr(str(b2mb(hitS.getMemoryUsage() + timeSeriesAddPolicy.memUsage))) << sep // num nodes in quad tree
            << fr(str(stopwatch.time()))      << sep << std::endl; // time to add all
        };

    Count showProgressStep = 1000000;

    Count count = 0;
    Count countProblems = 0;

    Point point;



//    ifstream is("/Users/lauro/projects/geo_pointset_tiles/build/x");
    FILE *filein = stdin;
    while (fread((void*) &point, sizeof(Point), 1, filein))
    {
        count++;

//        if (count == 10)
//            break;

        // if ((count % 1) == 0)
        // {
        //     std::clog << "Reading point: " << count << " problems: " << countProblems << " adds: " << hitS.getNumAdds() << std::endl;
        //     std::clog << "   point lat, long: " << point.latitude << ", " << point.longitude << std::endl;

        //     // unsigned char* ptr = (unsigned char*) &point;
        //     // fprintf(stderr,"\n------------> RECEIVED %3ld:   ", count);
        //     // for (int i=0;i<sizeof(Point);i++)
        //     //     fprintf(stderr,"%x ",ptr[i]);
        //     // fprintf(stderr,"\n\n");

        // }

        mercator::TileCoordinate tx, ty;
        mercator::MercatorProjection::tileOfLongitudeLatitude(point.longitude,
                                                              point.latitude,
                                                              LevelBits,
                                                              tx, ty);
        // tile is
        // std::cout << "coord: "
        //           << coords.longitude << "  "
        //           << coords.latitude << std::endl;
        // std::cout << "tile: " << tx << "  " << ty << std::endl;

        if (tx >= (1 << LevelBits) || (tx < 0) || ty >= (1 << LevelBits) || (ty < 0))
        {
            countProblems++;
            continue; // point outside bounds
        }

        QAddr addr((Coordinate) tx, (Coordinate) ty, LevelBits);

        // set address
        stree.setAddress(addr);

        // get timestamp
        static const Timestamp t0 = mkTimestamp(2010,1,1,0,0,0);
        static const Timestamp timeBinSizeInSeconds = timeBinSizeInHours * 60 * 60; // this is one day unit
        EventTime t = static_cast<EventTime>((point.timestamp - t0)/timeBinSizeInSeconds);

        // add timestamp to stree
        stree.add(t);

        if ((hitS.getNumAdds() % showProgressStep) == 0)
        {
            showProgress();
            // std::clog << hitS.report;
        }

        // std::cout << "hitS: " << hitS.getNumAdds() << std::endl;

    }

    // last report with totals
    showProgress();
    // std::clog << hitS.report;

    // computing statistics
    Stats<QuadTreeTS> stats;
    stats.initialize(hitS);
    stats.dumpReport(std::clog);

    // dump report on timeseries
    EventTimeSeries::dump_tslist(std::clog);

    // FIXME reimplement this

//    TimeSeriesStatisticsVisitor<LevelBits> statsVisitor;

//    { // run a count session
//        TimeSeriesStatistics::CountSessionRAII countSession(
//            statsVisitor.stats);

//        // get statistics of levels and node content storage
//        hitS.visitSubnodes(Address<LevelBits>(), -1, statsVisitor);

//    } // guarded call to reset and consolidate

//    statsVisitor.stats.dumpReport(std::clog);
//    statsVisitor.stats.histogram.dumpReport(std::clog);


//    // print time series of node 0
//    hitS.root->getContent()->dump(std::clog);


}


#if 0

typedef uint32_t TimeBin;
typedef uint32_t TSCount; // counter int type

#include <TimeSeries.hh>
#include <QuadTreeUtil.hh>
#include <QuadTreeNode.hh>
#include <QuadTree.hh>
#include <FlatTree.hh>

#include <TimeSeries.hh>

#include <stdint.h>

int main()
{

    typedef timeseries::TimeSeries<TimeBin, TSCount> TimeSeries;

    typedef boost::mpl::vector<
        quadtree::QuadTree<1, flattree::FlatTree< flattree::FlatTree<TimeSeries>>>,
        flattree::FlatTree< flattree::FlatTree<TimeSeries>>,
        flattree::FlatTree<TimeSeries >
        > PathListType;


    STree<PathListType, TimeBin> sTree;

    {
        quadtree::Address<1>  a0 = quadtree::Address<1>(0, 0, 1); // spatial address
        flattree::Address     a1 = flattree::Address(weekday::Mon); // spatial address
        flattree::Address     a2 = flattree::Address(10); // 10 o'clock
        sTree.setAddress(0, &a0);
        sTree.setAddress(1, &a1);
        sTree.setAddress(2, &a2);
        sTree.add(static_cast<TimeBin>(0));
    }

    {
        quadtree::Address<1>  a0 = quadtree::Address<1>(0, 0, 1); // spatial address
        flattree::Address     a1 = flattree::Address(weekday::Mon); // spatial address
        flattree::Address     a2 = flattree::Address(11); // 10 o'clock
        sTree.setAddress(0, &a0);
        sTree.setAddress(1, &a1);
        sTree.setAddress(2, &a2);
        sTree.add(static_cast<TimeBin>(0));
    }

    {
        quadtree::Address<1>  a0 = quadtree::Address<1>(0, 0, 1); // spatial address
        flattree::Address     a1 = flattree::Address(weekday::Tue); // spatial address
        flattree::Address     a2 = flattree::Address(10); // 10 o'clock
        sTree.setAddress(0, &a0);
        sTree.setAddress(1, &a1);
        sTree.setAddress(2, &a2);
        sTree.add(static_cast<TimeBin>(0));
    }

    {
        quadtree::Address<1>  a0 = quadtree::Address<1>(0, 0, 1); // spatial address
        flattree::Address     a1 = flattree::Address(weekday::Tue); // spatial address
        flattree::Address     a2 = flattree::Address(11); // 10 o'clock
        sTree.setAddress(0, &a0);
        sTree.setAddress(1, &a1);
        sTree.setAddress(2, &a2);
        sTree.add(static_cast<TimeBin>(0));
    }

    {
        quadtree::Address<1>  a0 = quadtree::Address<1>(0, 1, 1); // spatial address
        flattree::Address     a1 = flattree::Address(weekday::Tue); // spatial address
        flattree::Address     a2 = flattree::Address(11); // 10 o'clock
        sTree.setAddress(0, &a0);
        sTree.setAddress(1, &a1);
        sTree.setAddress(2, &a2);
        sTree.add(static_cast<TimeBin>(0));
    }

    // computing statistics
    quadtree::Stats<1> stats;
    stats.initialize(sTree.root);
    stats.dumpReport(std::clog);



    // // computing statistics
    // quadtree::Stats<25> stats;
    // stats.initialize(sTree.root);
    // stats.dumpReport(std::clog);

    // dispatcher d;

    // std::cout << typeid(types).name() << std::endl;
    // std::cout << typeid(mpl::next<types>::type::item).name() << std::endl;
    // std::cout << typeid(mpl::next<mpl::next<types>::type>::type).name() << std::endl;

    return 0;
}





//
// TimeSeries add and visit policy
//

template<BitSize N>
struct TimeSeriesAddPolicy
{

    // Signal that a node was added to a QuadTree at "address" while
    // in the process of adding "targetAddress" (targetAddress will
    // eventually be equal to address in this call)
    void newNode(Node<EventTimeSeries> *node,
                 Address<N> &,
                 Address<N> &)
    {
        // add to all nodes found
        node->setContent(new EventTimeSeries());
        memUsage += sizeof(EventTimeSeries);
    }

    // point was added
    void addPoint(Point                 &point,
                  Node<EventTimeSeries> *node,
                  Address<N>            &,
                  Address<N>            &)
    {
        EventTimeSeries* ts = node->getContent();

        static const Timestamp t0 = mkTimestamp(2010,1,1,0,0,0);
        static const Timestamp timeBinSizeInSeconds = timeBinSizeInHours * 60 * 60; // this is one day unit

        //
        EventTime t = static_cast<EventTime>((point.timestamp - t0)/timeBinSizeInSeconds);

        // truncate to integer unit of seconds

        memUsage += ts->add(t);

    }

    TimeSeriesAddPolicy(Timestamp timeBinSizeInHours):
        timeBinSizeInHours(timeBinSizeInHours),
        memUsage(0)
    {
        if (timeBinSizeInHours == 0)
            throw std::string("ooops");

    }

    Timestamp timeBinSizeInHours;
    Count     memUsage;

    TimeSeriesStatistics stats;

};

template<BitSize N>
struct TimeSeriesStatisticsVisitor
{
    void visit(Node<EventTimeSeries> *node, Address<N> addr)
    {
        EventTimeSeries *ts = node->getContent();
        stats.addLevelNodeInfo(addr.level, ts->getMemoryUsage());
        stats.histogram.add(ts->entries.size());
    }

    TimeSeriesStatistics stats;
};


// REPORT

void dumpTypeSizes(std::ostream &os)
{
    char sep = '|';

    os
        << sep
        << fl("Type")     << sep // number of add calls
        << fl("#Bytes")   << sep << std::endl;

    os  << sep
        << fl("NodeType0000")              << sep // number of add calls
        << fr(str(sizeof(quadtree::NodeType0000)))   << sep << std::endl;
    os  << sep
        << fl("NodeType1000")              << sep // number of add calls
        << fr(str(sizeof(quadtree::NodeType1000)))   << sep << std::endl;
    os  << sep
        << fl("NodeType1100")              << sep // number of add calls
        << fr(str(sizeof(quadtree::NodeType1100)))   << sep << std::endl;
    os  << sep
        << fl("NodeType1110")              << sep // number of add calls
        << fr(str(sizeof(quadtree::NodeType1110)))   << sep << std::endl;
    os  << sep
        << fl("NodeType1111")              << sep // number of add calls
        << fr(str(sizeof(quadtree::NodeType1110)))   << sep << std::endl;

    os  << sep
        << fl("QT")              << sep // number of add calls
        << fr(str(sizeof(quadtree::QuadTree<25,EventTimeSeries>))) << sep << std::endl;

    os  << sep
        << fl("TS")              << sep // number of add calls
        << fr(str(sizeof(EventTimeSeries)))   << sep << std::endl;

    os  << sep
        << fl("TS::Entry")              << sep // number of add calls
        << fr(str(sizeof(EventTimeSeries::Entry)))   << sep << std::endl;


    os  << sep
        << fl("SNode 0000")              << sep // number of add calls
        << fr(str(sizeof(quadtree::ScopedNode<int, quadtree::NodeType0000>)))   << sep << std::endl;
    os  << sep
        << fl("SNode 1000")              << sep // number of add calls
        << fr(str(sizeof(quadtree::ScopedNode<int, quadtree::NodeType1000>)))   << sep << std::endl;
    os  << sep
        << fl("SNode 0100")              << sep // number of add calls
        << fr(str(sizeof(quadtree::ScopedNode<int, quadtree::NodeType0100>)))   << sep << std::endl;
    os  << sep
        << fl("SNode 0010")              << sep // number of add calls
        << fr(str(sizeof(quadtree::ScopedNode<int, quadtree::NodeType0010>)))   << sep << std::endl;
    os  << sep
        << fl("SNode 0001")              << sep // number of add calls
        << fr(str(sizeof(quadtree::ScopedNode<int, quadtree::NodeType0001>)))   << sep << std::endl;
    os  << sep
        << fl("SNode 1100")              << sep // number of add calls
        << fr(str(sizeof(quadtree::ScopedNode<int, quadtree::NodeType1100>)))   << sep << std::endl;
    os  << sep
        << fl("SNode 1010")              << sep // number of add calls
        << fr(str(sizeof(quadtree::ScopedNode<int, quadtree::NodeType1010>)))   << sep << std::endl;
    os  << sep
        << fl("SNode 0110")              << sep // number of add calls
        << fr(str(sizeof(quadtree::ScopedNode<int, quadtree::NodeType0110>)))   << sep << std::endl;
    os  << sep
        << fl("SNode 1001")              << sep // number of add calls
        << fr(str(sizeof(quadtree::ScopedNode<int, quadtree::NodeType1001>)))   << sep << std::endl;
    os  << sep
        << fl("SNode 0101")              << sep // number of add calls
        << fr(str(sizeof(quadtree::ScopedNode<int, quadtree::NodeType0101>)))   << sep << std::endl;
    os  << sep
        << fl("SNode 0011")              << sep // number of add calls
        << fr(str(sizeof(quadtree::ScopedNode<int, quadtree::NodeType0011>)))   << sep << std::endl;
    os  << sep
        << fl("SNode 1110")              << sep // number of add calls
        << fr(str(sizeof(quadtree::ScopedNode<int, quadtree::NodeType1110>)))   << sep << std::endl;
    os  << sep
        << fl("SNode 1111")              << sep // number of add calls
        << fr(str(sizeof(quadtree::ScopedNode<int, quadtree::NodeType1111>)))   << sep << std::endl;


    os  << sep
        << fl("ChildIndex")              << sep // number of add calls
        << fr(str(sizeof(quadtree::ChildIndex)))   << sep << std::endl;
    os  << sep
        << fl("NodeKey")                 << sep // number of add calls
        << fr(str(sizeof(quadtree::NodeKey)))      << sep << std::endl;
    os  << sep
        << fl("Count")                   << sep // number of add calls
        << fr(str(sizeof(quadtree::Count)))        << sep << std::endl;
    os  << sep
        << fl("NumChildren")             << sep // number of add calls
        << fr(str(sizeof(quadtree::NumChildren)))  << sep << std::endl;

}

Count b2mb(Count b)
{
    Count mb = (Count) (b / (double) (1<<20));
    return mb;
}

ostream &operator<<(ostream &os, quadtree::MemUsage& memUsage)
{
    char sep = '|';
    os  << sep
        << fl("Type")     << sep // number of add calls
        << fl("0 Child.") << sep // number of add calls
        << fl("1 Child.") << sep // number of add calls
        << fl("2 Child.") << sep // number of add calls
        << fl("3 Child.") << sep // number of add calls
        << fl("4 Child.") << sep // number of add calls
        << fl("Total.")   << sep // number of add calls
        << std::endl;

    os  << sep
        << fl("Count") << sep // number of add calls
        << fr(str((memUsage.getCount(0)))) << sep // number of add calls
        << fr(str((memUsage.getCount(1)))) << sep // number of add calls
        << fr(str((memUsage.getCount(2)))) << sep // number of add calls
        << fr(str((memUsage.getCount(3)))) << sep // number of add calls
        << fr(str((memUsage.getCount(4)))) << sep // number of add calls
        << fr(str((memUsage.getCount()))) << sep // number of add calls
        << std::endl;

    os  << sep
        << fl("Mem.Use(MB)") << sep // number of add calls
        << fr(str(b2mb(memUsage.getMemUsage(0)))) << sep // number of add calls
        << fr(str(b2mb(memUsage.getMemUsage(1)))) << sep // number of add calls
        << fr(str(b2mb(memUsage.getMemUsage(2)))) << sep // number of add calls
        << fr(str(b2mb(memUsage.getMemUsage(3)))) << sep // number of add calls
        << fr(str(b2mb(memUsage.getMemUsage(4)))) << sep // number of add calls
        << fr(str(b2mb(memUsage.getMemUsage()))) << sep // number of add calls
        << std::endl;

    os  << sep
        << fl("Mem.Use(B)") << sep // number of add calls
        << fr(str((memUsage.getMemUsage(0)))) << sep // number of add calls
        << fr(str((memUsage.getMemUsage(1)))) << sep // number of add calls
        << fr(str((memUsage.getMemUsage(2)))) << sep // number of add calls
        << fr(str((memUsage.getMemUsage(3)))) << sep // number of add calls
        << fr(str((memUsage.getMemUsage(4)))) << sep // number of add calls
        << fr(str((memUsage.getMemUsage()))) << sep // number of add calls
        << std::endl;


    return os;
}

#endif