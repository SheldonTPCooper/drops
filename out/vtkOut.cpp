//**************************************************************************
// File:    vtkOut.cpp                                                     *
// Content: solution output in VTK legacy format                           *
// Author:  Sven Gross, Joerg Peters, Volker Reichelt, IGPM RWTH Aachen    *
//          Oliver Fortmeier, SC RWTH Aachen                               *
//**************************************************************************

#include "out/vtkOut.h"

namespace DROPS
{

VTKOutCL::VTKOutCL(const MultiGridCL& mg, const std::string& dataname, Uint numsteps,
            const std::string& filename, bool binary)
/** Beside constructing the VTKOutCL, this function computes the number of
    digits, that are used to decode the time steps in the filename.
\param mg        MultiGridCL that contains the geometry
\param dataname  name of the data
\param numsteps  number of time steps
\param filename  prefix of all files (e.g. vtk/output)
\param binary    Write out files in binary format. (Does not work)
\todo Support of binary output
*/
    : mg_(mg), timestep_(0), numsteps_(numsteps), descstr_(dataname),
        filename_(filename), binary_(binary), geomwritten_(false),
#ifndef _PAR
        vAddrMap_(), eAddrMap_(),
#else
        mgVersion_(-1u), tag_(3001), GIDAddrMap_(),
        procOffset_(ProcCL::Size()+1),
#endif
        coords_(), tetras_(),
        numPoints_(0), numTetras_(0)
{
    decDigits_= 1;
    while( numsteps>9){ ++decDigits_; numsteps/=10; }
}

void VTKOutCL::AppendTimecode( std::string& str) const
/** Append a timecode to the filename*/
{
    char format[]= "%0Xi",
         postfix[8];
    format[2]= '0' + char(decDigits_);
    std::sprintf( postfix, format, timestep_);
    str+= postfix;
    str+= ".vtk";
}

void VTKOutCL::CheckFile( const std::ofstream& os) const
/** Check if a file is open*/
{
    if (!os)
        throw DROPSErrCL( "VTKOutCL: error while opening file!");
}

void VTKOutCL::NewFile(double time)
/** Master process opens the new file and write header into it*/
{
    // new file is exclusivly created by the master process
    IF_NOT_MASTER
        return;

    std::string filename(filename_);
    AppendTimecode(filename);
    file_.open(filename.c_str());
    if (!file_){
        std::cerr << "Cannot open file "<<filename<<std::endl;
    }
    CheckFile( file_);
    PutHeader(time);
    wrotePointDataLine_= false;
}

void VTKOutCL::PutHeader(double time)
/** Write header into the vtk file*/
{
    file_ << "# vtk DataFile Version 2.0"  << '\n'
          << descstr_ << ", Time " << time << '\n'
          << (binary_?"BINARY":"ASCII")    << '\n'
          << "DATASET UNSTRUCTURED_GRID"   << '\n';
}

#ifndef _PAR
void VTKOutCL::GatherCoord()
/** Iterate over all vertices and edges and assign each vertex and edge a
    consecutive number and the coordinates */
{
    // Get number of vertices and edges in last triangulation level
    Uint numVertices=0, numEdges=0;

    for (MultiGridCL::const_TriangVertexIteratorCL it= mg_.GetTriangVertexBegin(); it!=mg_.GetTriangVertexEnd(); ++it)
        numVertices++;
    for (MultiGridCL::const_TriangEdgeIteratorCL it= mg_.GetTriangEdgeBegin(); it!=mg_.GetTriangEdgeEnd(); ++it)
        numEdges++;

    numPoints_= numLocPoints_= numVertices + numEdges;
    coords_.resize(3*numLocPoints_);

    Uint counter=0;
    for (MultiGridCL::const_TriangVertexIteratorCL it= mg_.GetTriangVertexBegin(); it!=mg_.GetTriangVertexEnd(); ++it){
        // store a constecutive number for the vertex
        vAddrMap_[&*it]= counter;

        // Put coordinate of the vertex into the field of coordinates
        for (int i=0; i<3; ++i)
            coords_[3*counter+i]= (float)it->GetCoord()[i];
        ++counter;
    }

    for (MultiGridCL::const_TriangEdgeIteratorCL it= mg_.GetTriangEdgeBegin(); it!=mg_.GetTriangEdgeEnd(); ++it){
        // store a constecutive number for the edge
        eAddrMap_[&*it]= counter;

        // Put coordinate of the barycenter of the edge into the field of coordinates
        const Point3DCL baryCenter= GetBaryCenter(*it);
        for (int i=0; i<3; ++i)
            coords_[3*counter+i]= (float)baryCenter[i];
        ++counter;
    }
}
#endif

#ifdef _PAR
void VTKOutCL::GatherCoord(VectorBaseCL<DDD_GID>& gidList, VectorBaseCL<float>& coordList)
/** This function only exists for the parallel version of DROPS. In sequential
    call this function without any arguments. <p>
    Iterate over all master tetrahedra and gather gids and coordinats.
    \param gidList   List of all gids that are stored on this processor
    \param coordList List of all coords that are stored on this processor
*/
{
    // Get number of exclusive vertices and edges
    Uint numExclusive=0;

    for (MultiGridCL::const_TriangVertexIteratorCL it= mg_.GetTriangVertexBegin(); it!=mg_.GetTriangVertexEnd(); ++it)
        if (AmIResponsible(*it))
            ++numExclusive;
    for (MultiGridCL::const_TriangEdgeIteratorCL it= mg_.GetTriangEdgeBegin(); it!=mg_.GetTriangEdgeEnd(); ++it)
        if (AmIResponsible(*it))
            ++numExclusive;
    // allocate memory for gids and coords
    gidList.resize(numExclusive);
    coordList.resize(3*numExclusive);
    numLocPoints_= numExclusive;

    Uint pos=0;

    // Gather Coords of vertices
    for (MultiGridCL::const_TriangVertexIteratorCL it= mg_.GetTriangVertexBegin(); it!=mg_.GetTriangVertexEnd(); ++it){
        if (AmIResponsible(*it)){
            gidList[pos]= it->GetGID();
            for (int i=0; i<3; ++i)
                coordList[3*pos+i]= (float)it->GetCoord()[i];
            ++pos;
        }
    }

    // Gather Coords of edges
    Point3DCL baryCenter;
    for (MultiGridCL::const_TriangEdgeIteratorCL it= mg_.GetTriangEdgeBegin(); it!=mg_.GetTriangEdgeEnd(); ++it){
        if (AmIResponsible(*it)){
            gidList[pos]= it->GetGID();
            baryCenter= GetBaryCenter(*it);
            for (int i=0; i<3; ++i)
                coordList[3*pos+i]= (float)baryCenter[i];
            ++pos;
        }
    }
}


void VTKOutCL::CommunicateCoords(const VectorBaseCL<DDD_GID>& gidList, const VectorBaseCL<float>& coordList)
/** <p> In sequential mode: Copy local list into global list and use the
    index-describer to map vertices/edges to an id.</p>
    <p> In parallel mode: Send coordinates and gids of exclusive vertices and
    edges to the master process. This process stores all cordinates and create a
    mapping gid->unique consecutive number </p>*/
{
    // Compute number of points
    numPoints_= ProcCL::GlobalSum( gidList.size(), ProcCL::Master());
    size_t maxPoints= ProcCL::GlobalMax( gidList.size(), ProcCL::Master());

    // Workers send gids and coords to master
    if (!ProcCL::IamMaster()){
        std::valarray<ProcCL::RequestT> req(2);
        req[0]= ProcCL::Isend(gidList,   ProcCL::Master(), tag_);
        req[1]= ProcCL::Isend(coordList, ProcCL::Master(), tag_+1);

        ProcCL::WaitAll(req);
    }

    // Master store all values
    if (ProcCL::IamMaster()){

        // Create memory
        coords_.resize(3*numPoints_);
        DDD_GID *recvGID= new DDD_GID[maxPoints];

        // Recieve Coords and GIDs from all processes
        int recievePos= 0, recieved=0, counter=0;
        for (int p=0; p<ProcCL::Size(); ++p){

            if (p!=ProcCL::MyRank()){   // if not master himself
                 // Get number of recieved gids and coord
                ProcCL::StatusT stat;
                ProcCL::Probe(p, tag_, stat);
                recieved = ProcCL::GetCount<DDD_GID>(stat);

                // Recieve GIDs and Coords
                ProcCL::Recv(recvGID, recieved, p, tag_);
                ProcCL::Recv(Addr(coords_)+recievePos, 3*recieved, p, tag_+1);
                recievePos+=3*recieved;

                // Assign each GID a number
                for (int i=0; i<recieved; ++i)
                    GIDAddrMap_[recvGID[i]]= counter++;
            }
            else{                       // master himself ;-)
                // copy local coords into global array
                std::copy(Addr(coordList), Addr(coordList)+coordList.size(), Addr(coords_)+recievePos);
                recievePos+= coordList.size();

                // Assign each GID a number
                for (Uint i=0; i<gidList.size(); ++i)
                    GIDAddrMap_[gidList[i]]= counter++;
            }
        }
        delete[] recvGID;
    }
}
#endif          // end of _PAR

void VTKOutCL::WriteCoords()
/** The master writes out the coordinates. */
{

    // Workes has to do nothing
    IF_NOT_MASTER
        return;

    // Write out coordinate part
    IF_MASTER
    {

        file_ << "POINTS " << numPoints_ << " float" << '\n';

        // write coordinates
        showFloat sFlo;
        for (Uint i=0; i<numPoints_; ++i){
            if (binary_){
                for (int j=0; j<3; ++j){
                    sFlo.f = coords_[3*i+j];
                    file_.write( sFlo.s, sizeof(float));
                }
            }
            else{
                file_ << coords_[3*i+0] << ' ' << coords_[3*i+1] << ' ' << coords_[3*i+2] << '\n';
            }
        }
        if (binary_)
            file_ << '\n';
    }
}

#ifndef _PAR
void VTKOutCL::GatherTetra()
/** Gather tetras in an array*/
{
    // Get number of tetras
    numTetras_=0;
    for (MultiGridCL::const_TriangTetraIteratorCL it= mg_.GetTriangTetraBegin(); it!=mg_.GetTriangTetraEnd(); ++it)
        numTetras_ += 8;                        // each tetra is stored reg-refined

    tetras_.resize(4*numTetras_);               // four vertices * numTetras

    // Gather connectivities
    Uint counter=0;
    for (MultiGridCL::const_TriangTetraIteratorCL it= mg_.GetTriangTetraBegin(); it!=mg_.GetTriangTetraEnd(); ++it){
        RefRuleCL RegRef= GetRefRule( RegRefRuleC);                                     // get regular refine rule
        for (int ch=0; ch<8; ++ch)                                                      // iterate over all children
        {
            ChildDataCL data= GetChildData( RegRef.Children[ch]);                       // datas of children
            for (int vert= 0; vert<4; ++vert)
            {
                int v= data.Vertices[vert];                                             // number of corresponding vert
                if (v<4)                                                                // number correspond to original vertex
                    tetras_[counter] = vAddrMap_[it->GetVertex(v)];
                else                                                                    // number correspond to an edge
                    tetras_[counter] = eAddrMap_[it->GetEdge(v-4)];
                counter++;
            }
        }
    }
    Assert(counter==4*numTetras_, DROPSErrCL("VTKOutCL::GatherTetra: Mismatching number of tetras"), ~0);
}


#else           // _PAR

void VTKOutCL::GatherTetra(VectorBaseCL<DDD_GID>& locConnectList) const
/** Gather connectivities of local tetras in a field. This function is only
    present in the parallel version of DROPS.
\param locConnectList List of local connectivities*/
{
    // Count number of tetras
    Uint numTetra=0;
    for (MultiGridCL::const_TriangTetraIteratorCL it= mg_.GetTriangTetraBegin(); it!=mg_.GetTriangTetraEnd(); ++it)
        numTetra+=8;

    // allocate memory for tetras
    locConnectList.resize(4*numTetra);
    Uint pos=0;

    // Gather connectivities
    for (MultiGridCL::const_TriangTetraIteratorCL it= mg_.GetTriangTetraBegin(); it!=mg_.GetTriangTetraEnd(); ++it){
        RefRuleCL RegRef= GetRefRule( RegRefRuleC);                                     // get regular refine rule
        for (int ch=0; ch<8; ++ch)                                                      // iterate over all children
        {
            ChildDataCL data= GetChildData( RegRef.Children[ch]);                       // datas of children
            for (int vert= 0; vert<4; ++vert)
            {
                int v= data.Vertices[vert];                                             // number of corresponding vert
                if (v<4)                                                                // number correspond to original vertex
                    locConnectList[pos] = it->GetVertex(v)->GetGID();
                else                                                                    // number correspond to an edge
                    locConnectList[pos] = it->GetEdge(v-4)->GetGID();
                pos++;
            }
        }
    }
    Assert(pos==4*numTetra, DROPSErrCL("VTKOutCL::GatherTetra: Mismatching number of tetras"), ~0);
}

void VTKOutCL::CommunicateTetra(const VectorBaseCL<DDD_GID>& locConnectList)
/** Send all local connectivities to master or copy in sequiental mode.*/
{
    // get number of tetras
    numTetras_= ProcCL::GlobalSum( locConnectList.size()/4, ProcCL::Master());
    // Workers send tetra list to master
    if (!ProcCL::IamMaster()){
        ProcCL::RequestT req_tetra  = ProcCL::Isend(locConnectList, ProcCL::Master(), tag_+2);
        ProcCL::Wait(req_tetra);
    }

    // Recieve lists
    if (ProcCL::IamMaster()){
        // Allocate mem for tetras
        tetras_.resize(4*numTetras_);

        int recievePos=0, recieved=0;
        // if distribution should written out, remember, where proc p stores its tetras
        procOffset_[0]= 0;
        for (int p=0; p<ProcCL::Size(); ++p){       // Receive from all processors
            if (p!=ProcCL::MyRank()){               // if not master

                // Get number of recieved tetras
                ProcCL::StatusT stat;
                ProcCL::Probe(p, tag_+2, stat);
                recieved = ProcCL::GetCount<DDD_GID>(stat);
                // Recieve tetras
                ProcCL::Recv(Addr(tetras_)+recievePos, recieved, p, tag_+2);
                // increment next free position
                recievePos+= recieved;
                // remember, where proc p stores its tetras
                procOffset_[p+1]= procOffset_[p] + recieved/4;
            }
            else{                                   // master: store local array in recieve array
                std::copy(Addr(locConnectList), Addr(locConnectList)+locConnectList.size(), Addr(tetras_)+recievePos);
                // increment next free position
                recievePos+= locConnectList.size();
                procOffset_[p+1]= procOffset_[p] + locConnectList.size()/4;
            }
        }
    }
}
#endif          // of _PAR

void VTKOutCL::WriteTetra()
/** Write tetras into the vtk file*/
{
    // Workers have to do nothing
    IF_NOT_MASTER
        return;

    // Write out connectivities
    IF_MASTER
    {
        file_ << '\n' << "CELLS " << numTetras_ << ' ' << (5*numTetras_) << '\n';

        showInt sInt;

        // Write out connectivities
        for (Uint i=0; i<numTetras_; ++i){
            if (binary_){
#ifdef _PAR
                sInt.i = 4;
                file_.write( sInt.s, sizeof(int));
                for (int j=0; j<4; ++j){
                    sInt.i= (int)GIDAddrMap_[tetras_[4*i+j]];
                    file_.write( sInt.s, sizeof(int));
                }
#endif
            }
            else{
#ifndef _PAR
                file_ << '4' << ' '
                      << tetras_[4*i+0] << ' '
                      << tetras_[4*i+1] << ' '
                      << tetras_[4*i+2] << ' '
                      << tetras_[4*i+3] << '\n';
#else
                file_ << '4' << ' '
                      << GIDAddrMap_[tetras_[4*i+0]] << ' '
                      << GIDAddrMap_[tetras_[4*i+1]] << ' '
                      << GIDAddrMap_[tetras_[4*i+2]] << ' '
                      << GIDAddrMap_[tetras_[4*i+3]] << '\n';
#endif
            }
        }

        file_ << '\n' << "CELL_TYPES " << numTetras_ << '\n';

        const int tetraType= 10;
        sInt.i = tetraType;
        for (Uint i=0; i<numTetras_; ++i){
            if (binary_)
                file_.write( sInt.s, sizeof(int));
            else
                file_ << tetraType << '\n';
        }
        if (binary_)
            file_ << '\n';
    }
}

#ifdef _PAR
void VTKOutCL::WriteDistribution()
{
    Uint tmp_counter=0;
    showInt sInt;

    IF_MASTER{
        file_ << '\n' << "CELL_DATA " << numTetras_ << '\n'
            << "SCALARS distribution int 1 \nLOOKUP_TABLE default\n";
        for (int p=0; p<ProcCL::Size(); ++p){
            for (Uint t=procOffset_[p]; t<procOffset_[p+1]; ++t){
                sInt.i= p;
                if (binary_)
                    file_.write( sInt.s, sizeof(int));
                else
                    file_ << p << '\n';
                tmp_counter++;
            }
        }
        Assert(tmp_counter==numTetras_, DROPSErrCL("VTKOutCL::WriteDistribution: Wrong number of tetrahedra"), DebugOutPutC);
    }
}
#endif


#ifdef _PAR
void VTKOutCL::CommunicateValues(const VectorBaseCL<float>& locData, VectorBaseCL<float>& allData, int numData)
/** all processors send data to master or copy in sequiental mode*/
{
    // worker processes send data to master
    if (!ProcCL::IamMaster()){
        ProcCL::RequestT req_values= ProcCL::Isend(locData, ProcCL::Master(), tag_);
        ProcCL::Wait(req_values);
    }

    // master collect all data
    if (ProcCL::IamMaster()){

        // Allocate mem
        allData.resize(numPoints_ * numData);

        int recievePos=0, recieved=0;

        for (int p=0; p<ProcCL::Size(); ++p){

            if(p!=ProcCL::MyRank()){                // recieve from worker
                // Get number of recieved values
                ProcCL::StatusT stat;
                ProcCL::Probe(p, tag_, stat);
                recieved = ProcCL::GetCount<float>(stat);

                // Recieve values
                ProcCL::Recv(Addr(allData)+recievePos, recieved, p, tag_);
                recievePos+= recieved;
            }
            else{                                   // copy local data
                std::copy(Addr(locData), Addr(locData)+locData.size(), Addr(allData)+recievePos);
                recievePos+= locData.size();
            }
        }
    }
}
#endif

void VTKOutCL::WriteValues(const VectorBaseCL<float>& allData, const std::string& name, int numData)
/** master writes out data*/
{
    // workers have to do nothing
    IF_NOT_MASTER
        return;

    IF_MASTER
    {

        // write information into vtk file
        if (!wrotePointDataLine_){
            file_ << "POINT_DATA " << numPoints_ << '\n';
            wrotePointDataLine_= true;
        }

        if (numData==1){
            file_ << "SCALARS "<< name << " float" << '\n'
                  << "LOOKUP_TABLE default" << '\n';
        }
        else if (numData==3){
            file_ << "VECTORS "<< name <<" float" << '\n';
        }
        else
            throw DROPSErrCL("VTKOutCL::WriteScalar: Only scalar and vector valued functions!");

        // Write values
        showInt sInt;
        for (Uint i=0; i<allData.size(); ++i){
            if (binary_){
                sInt.i= (int)allData[i];
                file_.write( sInt.s, sizeof(int));
            }
            else{
                file_ << allData[i] << ' ';
                if (i%6==5) file_ << '\n';
            }
        }
        file_ << "\n";
    }
}

void VTKOutCL::PutGeom(double time, __UNUSED__  bool writeDistribution)
/** At first the geometry is put into the VTK file. Therefore this procedure
    opens the file and write description into the file.
    \param time simulation time
    \param writeDistribution Distribution is written as CELL_DATA
*/
{
    NewFile(time);

#ifndef _PAR
    Clear();
    GatherCoord();
    GatherTetra();
#else
    // multigrid has changed, so compute GIDs, Coords and tetras new (only works for parallel version)
    if (mgVersion_ != mg_.GetVersion()){
        // Clear all information
        Clear();

        // Collect Coords and GIDs as well as tetra information
        VectorBaseCL<DDD_GID>  locGidList;
        VectorBaseCL<float> locCoordList;
        GatherCoord( locGidList, locCoordList);
        CommunicateCoords( locGidList, locCoordList);

        VectorBaseCL<DDD_GID> locConnectList;
        GatherTetra( locConnectList);
        CommunicateTetra( locConnectList);

        // Store mg version number of last written out mg
        mgVersion_= mg_.GetVersion();
    }
#endif
    // Write out coordinates and tetras
    WriteCoords();
    WriteTetra();
#ifdef _PAR
    if (writeDistribution)
        WriteDistribution();
#endif
}

void VTKOutCL::Clear()
{
    if (numPoints_>0){
#ifndef _PAR
        vAddrMap_.clear();
        eAddrMap_.clear();
#else
        GIDAddrMap_.clear();
        procOffset_.clear();
#endif
        coords_.resize(0);
        tetras_.resize(0);
    }
}

} // end of namespace DROPS
