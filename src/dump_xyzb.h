/* -*- c++ -*- ----------------------------------------------------------
   LAMMPS - Large-scale Atomic/Molecular Massively Parallel Simulator
   https://www.lammps.org/, Sandia National Laboratories
   Steve Plimpton, sjplimp@sandia.gov

   Copyright (2003) Sandia Corporation.  Under the terms of Contract
   DE-AC04-94AL85000 with Sandia Corporation, the U.S. Government retains
   certain rights in this software.  This software is distributed under
   the GNU General Public License.

   See the README file in the top-level LAMMPS directory.
------------------------------------------------------------------------- */

#ifdef DUMP_CLASS
// clang-format off
DumpStyle(xyzb,DumpXYZB);
// clang-format on
#else

#ifndef LMP_DUMP_XYZB_H
#define LMP_DUMP_XYZB_H

#include "dump.h"

namespace LAMMPS_NS {

class DumpXYZB : public Dump {
 public:
  DumpXYZB(class LAMMPS *, int, char **);
  ~DumpXYZB() override;

 protected:
  int ntypes;
  char **typenames;

  void init_style() override;
  void write_header(bigint) override;
  void pack(tagint *) override;
  int convert_string(int, double *) override;
  void write_data(int, double *) override;
  int modify_param(int, char **) override;

  typedef void (DumpXYZB::*FnPtrWrite)(int, double *);
  FnPtrWrite write_choice;    // ptr to write data functions
  void write_string(int, double *);
  void write_lines(int, double *);
};

}    // namespace LAMMPS_NS

#endif
#endif

/* ERROR/WARNING messages:

E: Illegal ... command

Self-explanatory.  Check the input script syntax and compare to the
documentation for the command.  You can use -echo screen as a
command-line option when running LAMMPS to see the offending line.

E: Invalid dump xyz filename

Filenames used with the dump xyz style cannot be binary or cause files
to be written by each processor.

E: Dump modify element names do not match atom types

Number of element names must equal number of atom types.

*/
