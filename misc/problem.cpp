//**************************************************************************
// File:    problem.cpp                                                    *
// Content: classes that constitute a problem                              *
// Author:  Sven Gross, Joerg Peters, Volker Reichelt, IGPM RWTH Aachen    *
// Version: 0.1                                                            *
// History: begin - March, 16 2001                                         *
//**************************************************************************

#include "misc/problem.h"

namespace DROPS
{

void IdxDescCL::Set(Uint idxnum, Uint unkVertex, Uint unkEdge, Uint unkFace, Uint unkTetra)
// sets up the number of unknowns in every subsimplex for a certain index.
// Remark: expects _IdxDesc to be long enough
{
    NumUnknownsVertex = unkVertex;
    NumUnknownsEdge   = unkEdge;
    NumUnknownsFace   = unkFace;
    NumUnknownsTetra  = unkTetra;
    Idx               = idxnum;
}

void MatDescCL::SetIdx(IdxDescCL* row, IdxDescCL* col)
{
    // create new matrix and set up the matrix description
    RowIdx= row;
    ColIdx= col;
}

void MatDescCL::Reset()
{
    RowIdx = 0;
    ColIdx = 0;
    Data.clear();
}

void CreateNumbOnTetra( const Uint idx, IdxT& counter, Uint stride,
                        const MultiGridCL::TriangTetraIteratorCL& begin,
                        const MultiGridCL::TriangTetraIteratorCL& end)
// allocates memory for the Unknown-indices on all simplices between begin and end 
// and numbers them in order of appearence.
{
    if (stride == 0) return;
    for (MultiGridCL::TriangTetraIteratorCL it=begin; it!=end; ++it)
    {
        it->Unknowns.Prepare( idx);
        it->Unknowns(idx)= counter;
        counter+= stride;
    }
}

} // end of namespace DROPS
