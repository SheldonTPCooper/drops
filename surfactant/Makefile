#############################################
#   D R O P S   local makefile              #
#############################################

# variables:

DIR = surfactant

EXEC = surfactant surfacenorms

DELETE = $(EXEC)

CPP = $(wildcard *.cpp)

OBJ = $(CPP:%.cpp=%.o)


# rules

default: ClrScr all

surfactant: \
    ../surfactant/surfactant.o ../geom/boundary.o ../geom/builder.o ../geom/simplex.o ../geom/multigrid.o \
    ../num/unknowns.o ../geom/topo.o ../num/fe.o ../misc/problem.o ../levelset/levelset.o \
    ../misc/utils.o ../out/output.o ../num/discretize.o ../misc/params.o ../num/interfacePatch.o \
    ../levelset/fastmarch.o ../surfactant/ifacetransp.o \
    ../num/fe.o ../out/ensightOut.o ../levelset/surfacetension.o ../out/vtkOut.o \
    ../geom/principallattice.o ../geom/reftetracut.o ../geom/subtriangulation.o ../num/quadrature.o
	$(CXX) -o $@ $^ $(LFLAGS)

surfacenorms: \
    ../surfactant/surfacenorms.o ../geom/boundary.o ../geom/builder.o ../geom/simplex.o ../geom/multigrid.o \
    ../num/unknowns.o ../geom/topo.o ../num/fe.o ../misc/problem.o ../levelset/levelset.o \
    ../misc/utils.o ../out/output.o ../num/discretize.o ../misc/params.o ../num/interfacePatch.o \
    ../levelset/fastmarch.o ../surfactant/ifacetransp.o \
    ../num/fe.o ../out/ensightOut.o ../levelset/surfacetension.o ../out/vtkOut.o \
    ../geom/principallattice.o ../geom/reftetracut.o ../geom/subtriangulation.o ../num/quadrature.o
	$(CXX) -o $@ $^ $(LFLAGS)

# include common rules:
include ../Make.common
