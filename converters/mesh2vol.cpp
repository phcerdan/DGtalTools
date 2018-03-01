/**
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as
 *  published by the Free Software Foundation, either version 3 of the
 *  License, or  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 **/
/**
 * @file mesh2vol.cpp
 * @ingroup converters
 *
 * @date 2018/01/11
 *
 *
 *
 * This file is part of the DGtalTools.
 */

///////////////////////////////////////////////////////////////////////////////
#include <iostream>
#include <fstream>
#include <chrono>
#include "DGtal/helpers/StdDefs.h"
#include "DGtal/shapes/MeshVoxelizer.h"
#include "DGtal/kernel/sets/CDigitalSet.h"
#include "DGtal/kernel/domains/HyperRectDomain.h"
#include "DGtal/io/readers/MeshReader.h"
#include "DGtal/io/Display3D.h"
#include "DGtal/io/writers/GenericWriter.h"
#include "DGtal/images/ImageContainerBySTLVector.h"
#include <boost/program_options/options_description.hpp>
#include <boost/program_options/parsers.hpp>
#include <boost/program_options/variables_map.hpp>

using namespace std;
using namespace DGtal;

///////////////////////////////////////////////////////////////////////////////
namespace po = boost::program_options;

/**
 @page mesh2vol
 @brief Convert a mesh file into a 26-separated or 6-separated voxelization in a given resolution grid.

@b Usage: mesh2vol [input]

@b Allowed @b options @b are:

@code
  -h [ --help ]                   display this message
  -i [ --input ] arg              mesh file (.off)
  -o [ --output ] arg             filename of ouput volumetric file (vol, pgm3d, ...)
                                  (auto-generated by argument values if empty)
  -s [ --separation ] arg (=6)    voxelization 6-separated or 26-separated.
  -r [ --resolution ]             digitization domain size (e.g. 128). The mesh will be scaled such that its bounding box maps to [0,resolution)^3.

@endcode

@b Example:
@code
  $ mesh2vol -i ${DGtal}/examples/samples/tref.off --separation 26 --resolution 256 -o output.vol
@endcode

@see mesh2vol.cpp

*/

template< unsigned int SEP >
void voxelizeAndExport(const std::string& inputFilename,
                       const std::string& outputFilename,
                       const unsigned int resolution)
{
  using Domain   = Z3i::Domain;
  using PointR3  = Z3i::RealPoint;
  using PointZ3  = Z3i::Point;

  trace.beginBlock("Preparing the mesh");
  trace.info() << "Reading input file: " << inputFilename;
  Mesh<PointR3> inputMesh;
  MeshReader<PointR3>::importOFFFile(inputFilename.c_str(), inputMesh);
  trace.info() << " [done]" << std::endl;
  const std::pair<PointR3, PointR3> bbox = inputMesh.getBoundingBox();
  trace.info()<< "Mesh bounding box: "<<bbox.first <<" "<<bbox.second<<std::endl;

  const double smax = (bbox.second - bbox.first).max();
  const double factor = resolution / smax;
  const PointR3 translate = -bbox.first;
  trace.info() << "Scale = "<<factor<<" translate = "<<translate<<std::endl;
  for(auto it = inputMesh.vertexBegin(), itend = inputMesh.vertexEnd();
      it != itend; ++it)
  {
    //scale + translation
    *it += translate;
    *it *= factor;
  }
  trace.endBlock();
  
  trace.beginBlock("Voxelization");
  trace.info() << "Voxelization " << SEP << "-separated ; " << resolution << "^3 ";
  Domain aDomain(PointZ3().diagonal(0), PointZ3().diagonal(resolution));
  
  //Digitization step
  Z3i::DigitalSet mySet(aDomain);
  MeshVoxelizer<Z3i::DigitalSet, SEP> aVoxelizer;
  aVoxelizer.voxelize(mySet, inputMesh, 1.0);
  trace.info() << " [done] " << std::endl;
  trace.endBlock();
  
  trace.beginBlock("Exporting");
  // Export the digital set to a vol file
  trace.info()<<aDomain<<std::endl;
  ImageContainerBySTLVector<Domain, unsigned char> image(aDomain);
  for(auto p: mySet)
    image.setValue(p, 128);
  image >> outputFilename.c_str();
  trace.endBlock();
}

int main( int argc, char** argv )
{ 
  // parse command line ----------------------------------------------
  po::options_description general_opt("\nAllowed options are");
  general_opt.add_options()
    ("help,h", "display this message")
    ("input,i", po::value<std::string>(), "mesh file (.off) " )
    ("output,o", po::value<std::string>(), "filename of ouput volumetric file (vol, pgm3d, ...).")
    ("separation,s", po::value<unsigned int>()->default_value(6), "voxelization 6-separated or 26-separated." )
    ("resolution,r", po::value<unsigned int>(), "digitization domain size (e.g. 128). The mesh will be scaled such that its bounding box maps to [0,resolution)^3." );

  bool parseOK=true;
  po::variables_map vm;
  try {
    po::store(po::parse_command_line(argc, argv, general_opt), vm);
  } catch(const std::exception& ex) {
    parseOK=false;
    trace.info() << "Error checking program options: " << ex.what() << endl;
  }

  po::notify(vm);

  if(!parseOK || vm.count("help") || argc < 1)
  {
    std::cout << "Usage: " << argv[0] << " -i [input]\n"
              << "Convert a mesh file into a 26-separated or 6-separated volumetric voxelization in a given resolution grid."
              << general_opt << "\n";
    std::cout << "Example:\n"
              << "mesh2vol -i ${DGtal}/examples/samples/tref.off -o output.vol --separation 26 --resolution 256 \n";
    return -1;
  }
  
  if( !vm.count("input") )
  {
    trace.error() << " Input filename is needed to be defined" << endl;
    return -1;
  }
  
  if( vm["separation"].as<unsigned int>() != 6 &&
      vm["separation"].as<unsigned int>() != 26 )
  {
    trace.error() << " Separation should be 6 or 26" << endl;
    return -1;
  }

  unsigned int separation = vm["separation"].as<unsigned int>();
  unsigned int resolution;
  if (vm.count("resolution"))
    resolution = vm["resolution"].as<unsigned int>();
  else
  {
    trace.error()<<"Missing output parameter '--resolution'"<<std::endl;
    return -1;
  }
  
  string inputFilename = vm["input"].as<std::string>();
  string outputFilename;
  
  if( vm.count("output") )
    outputFilename = vm["output"].as<std::string>();
  else
  {
    trace.error()<<"Missing output parameter '--output'"<<std::endl;
    return -1;
  }
  
  
  if(separation == 6)
    voxelizeAndExport<6>(inputFilename, outputFilename, resolution);
  else if(separation == 26)
    voxelizeAndExport<26>(inputFilename, outputFilename, resolution);

  return 0;
}

