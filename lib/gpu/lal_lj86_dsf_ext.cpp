/***************************************************************************
                               lj_dsf_ext.cpp
                             -------------------
                            W. Michael Brown (ORNL)

  Functions for LAMMPS access to lj/cut/coul/dsf acceleration routines.

 __________________________________________________________________________
    This file is part of the LAMMPS Accelerator Library (LAMMPS_AL)
 __________________________________________________________________________

    begin                : 7/12/2012
    email                : brownw@ornl.gov
 ***************************************************************************/

#include <iostream>
#include <cassert>
#include <cmath>

#include "lal_lj86_dsf.h"

using namespace std;
using namespace LAMMPS_AL;

static LJ86DSF<PRECISION,ACC_PRECISION> LJ86DMF;

// ---------------------------------------------------------------------------
// Allocate memory on host and device and copy constants to device
// ---------------------------------------------------------------------------
int lj86d_gpu_init(const int ntypes, double **cutsq, double **host_lj1,
                 double **host_lj2, double **host_lj3, double **host_lj4,
                 double **offset, double *special_lj, const int inum,
                 const int nall, const int max_nbors, const int maxspecial,
                 const double cell_size, int &gpu_mode, FILE *screen,
                 double **host_cut_ljsq, const double host_cut_coulsq,
                 double *host_special_coul, const double qqrd2e,
                 const double e_shift, const double f_shift,
                 const double alpha) {
  LJ86DMF.clear();
  gpu_mode=LJ86DMF.device->gpu_mode();
  double gpu_split=LJ86DMF.device->particle_split();
  int first_gpu=LJ86DMF.device->first_device();
  int last_gpu=LJ86DMF.device->last_device();
  int world_me=LJ86DMF.device->world_me();
  int gpu_rank=LJ86DMF.device->gpu_rank();
  int procs_per_gpu=LJ86DMF.device->procs_per_gpu();

  LJ86DMF.device->init_message(screen,"lj86/cut/coul/dsf",first_gpu,last_gpu);

  bool message=false;
  if (LJ86DMF.device->replica_me()==0 && screen)
    message=true;

  if (message) {
    fprintf(screen,"Initializing Device and compiling on process 0...");
    fflush(screen);
  }

  int init_ok=0;
  if (world_me==0)
    init_ok=LJ86DMF.init(ntypes, cutsq, host_lj1, host_lj2, host_lj3,
                       host_lj4, offset, special_lj, inum, nall, max_nbors,
                       maxspecial, cell_size, gpu_split, screen, host_cut_ljsq,
                       host_cut_coulsq, host_special_coul, qqrd2e, e_shift,
                       f_shift, alpha);

  LJ86DMF.device->world_barrier();
  if (message)
    fprintf(screen,"Done.\n");

  for (int i=0; i<procs_per_gpu; i++) {
    if (message) {
      if (last_gpu-first_gpu==0)
        fprintf(screen,"Initializing Device %d on core %d...",first_gpu,i);
      else
        fprintf(screen,"Initializing Devices %d-%d on core %d...",first_gpu,
                last_gpu,i);
      fflush(screen);
    }
    if (gpu_rank==i && world_me!=0)
      init_ok=LJ86DMF.init(ntypes, cutsq, host_lj1, host_lj2, host_lj3, host_lj4,
                         offset, special_lj, inum, nall, max_nbors, maxspecial,
                         cell_size, gpu_split, screen, host_cut_ljsq,
                         host_cut_coulsq, host_special_coul, qqrd2e, e_shift,
                         f_shift, alpha);

    LJ86DMF.device->gpu_barrier();
    if (message)
      fprintf(screen,"Done.\n");
  }
  if (message)
    fprintf(screen,"\n");

  if (init_ok==0)
    LJ86DMF.estimate_gpu_overhead();
  return init_ok;
}

void lj86d_gpu_clear() {
  LJ86DMF.clear();
}

int** lj86d_gpu_compute_n(const int ago, const int inum_full,
                        const int nall, double **host_x, int *host_type,
                        double *sublo, double *subhi, tagint *tag, int **nspecial,
                        tagint **special, const bool eflag, const bool vflag,
                        const bool eatom, const bool vatom, int &host_start,
                        int **ilist, int **jnum, const double cpu_time,
                        bool &success, double *host_q, double *boxlo,
                        double *prd) {
  return LJ86DMF.compute(ago, inum_full, nall, host_x, host_type, sublo,
                       subhi, tag, nspecial, special, eflag, vflag, eatom,
                       vatom, host_start, ilist, jnum, cpu_time, success,
                       host_q, boxlo, prd);
}

void lj86d_gpu_compute(const int ago, const int inum_full, const int nall,
                     double **host_x, int *host_type, int *ilist, int *numj,
                     int **firstneigh, const bool eflag, const bool vflag,
                     const bool eatom, const bool vatom, int &host_start,
                     const double cpu_time, bool &success, double *host_q,
                     const int nlocal, double *boxlo, double *prd) {
  LJ86DMF.compute(ago,inum_full,nall,host_x,host_type,ilist,numj,firstneigh,eflag,
                vflag,eatom,vatom,host_start,cpu_time,success,host_q,
                nlocal,boxlo,prd);
}

double lj86d_gpu_bytes() {
  return LJ86DMF.host_memory_usage();
}


