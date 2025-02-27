// clang-format off
/* ----------------------------------------------------------------------
   LAMMPS - Large-scale Atomic/Molecular Massively Parallel Simulator
   https://www.lammps.org/, Sandia National Laboratories
   Steve Plimpton, sjplimp@sandia.gov

   Copyright (2003) Sandia Corporation.  Under the terms of Contract
   DE-AC04-94AL85000 with Sandia Corporation, the U.S. Government retains
   certain rights in this software.  This software is distributed under
   the GNU General Public License.

   See the README file in the top-level LAMMPS directory.
------------------------------------------------------------------------- */

/* ----------------------------------------------------------------------
   Contributing author:  Yuansheng Zhao
------------------------------------------------------------------------- */

#include "pair_table_compute_gpu.h"

#include "atom.h"
#include "domain.h"
#include "error.h"
#include "force.h"
#include "gpu_extra.h"
#include "memory.h"
#include "neigh_list.h"
#include "neigh_request.h"
#include "neighbor.h"
#include "suffix.h"
#include "compute.h"
#include "comm.h"

#include <cmath>

using namespace LAMMPS_NS;

int xndaf_gpu_init(const int ntypes, const int ntable, double host_cutsq,
           double host_dr, double *host_special_lj,
           const int inum, const int nall, const int max_nbors,
           const int maxspecial, const double cell_size,
           int &gpu_mode, FILE *screen);
void xndaf_gpu_clear();
void xndaf_gpu_sendTable(double **tb);
int ** xndaf_gpu_compute_n(const int ago, const int inum_full,
                           const int nall, double **host_x, int *host_type,
                           double *sublo, double *subhi, tagint *tag, int **nspecial,
                           tagint **special, const bool eflag, const bool vflag,
                           const bool eatom, const bool vatom, int &host_start,
                           int **ilist, int **jnum, const double cpu_time,
                           bool &success);
void xndaf_gpu_compute(const int ago, const int inum_full, const int nall,
                       double **host_x, int *host_type, int *ilist, int *numj,
                       int **firstneigh, const bool eflag, const bool vflag,
                       const bool eatom, const bool vatom, int &host_start,
                       const double cpu_time, bool &success);
double xndaf_gpu_bytes();                                            

/* ---------------------------------------------------------------------- */

PairTBCOMPGPU::PairTBCOMPGPU(LAMMPS *lmp) : PairTBCOMP(lmp), gpu_mode(GPU_FORCE)
{
  respa_enable = 0;
  reinitflag = 0;
  cpu_time = 0.0;
  suffix_flag |= Suffix::GPU;
  GPU_EXTRA::gpu_ready(lmp->modify, lmp->error);
}

PairTBCOMPGPU::~PairTBCOMPGPU()
{
  xndaf_gpu_clear();
}


void PairTBCOMPGPU::compute(int eflag, int vflag)
{
  int i,j;
  ev_init(eflag, vflag);

  int nall = atom->nlocal + atom->nghost;
  int inum, host_start;

  bool success = true;
  int *ilist, *numneigh, **firstneigh;

  // calc sq and generate force table;
  if(ncall%update_interval==0){
    force_compute -> compute_array();
    xndaf_gpu_sendTable(frc);
  }

  ncall++;

  if (gpu_mode != GPU_FORCE) {
    double sublo[3],subhi[3];
    if (domain->triclinic == 0) {
      sublo[0] = domain->sublo[0];
      sublo[1] = domain->sublo[1];
      sublo[2] = domain->sublo[2];
      subhi[0] = domain->subhi[0];
      subhi[1] = domain->subhi[1];
      subhi[2] = domain->subhi[2];
    } else {
      domain->bbox(domain->sublo_lamda,domain->subhi_lamda,sublo,subhi);
    }
    inum = atom->nlocal;
    firstneigh = xndaf_gpu_compute_n(neighbor->ago, inum, nall, atom->x,
                                     atom->type, sublo, subhi,
                                     atom->tag, atom->nspecial, atom->special,
                                     eflag, vflag, eflag_atom, vflag_atom,
                                     host_start, &ilist, &numneigh, cpu_time,
                                     success);
  } else {
    inum = list->inum;
    ilist = list->ilist;
    numneigh = list->numneigh;
    firstneigh = list->firstneigh;
    xndaf_gpu_compute(neighbor->ago, inum, nall, atom->x, atom->type,
                      ilist, numneigh, firstneigh, eflag, vflag, eflag_atom,
                      vflag_atom, host_start, cpu_time, success);
  }
  if (!success)
    error->one(FLERR,"Insufficient memory on accelerator");

  if (host_start<inum) {
    cpu_time = platform::walltime();
    cpu_compute(host_start, inum, eflag, vflag, ilist, numneigh, firstneigh);
    cpu_time = platform::walltime() - cpu_time;
  }

  if (eflag) {
    eng_vdwl=**frc;
  }
}

void PairTBCOMPGPU::init_style()
{
  ncall=0;
  // Repeat cutsq calculation because done after call to init_style
  if (!allocated) error->all(FLERR,"All pair coeffs are not set");
  double cell_size = force_cutoff + neighbor->skin;

  int maxspecial=0;
  if (atom->molecular != Atom::ATOMIC)
    maxspecial=atom->maxspecial;
  int mnf = 5e-2 * neighbor->oneatom;
  int success = xndaf_gpu_init(atom->ntypes+1, nbin_r, force_cutoff_sq, ddr,
                             force->special_lj, atom->nlocal,
                             atom->nlocal+atom->nghost, mnf, maxspecial,
                             cell_size, gpu_mode, screen);
  GPU_EXTRA::check_flag(success,error,world);

  if (gpu_mode == GPU_FORCE) {
    int irequest = neighbor->request(this,instance_me);
    neighbor->requests[irequest]->half = 0;
    neighbor->requests[irequest]->full = 1;
  }
  else error->all(FLERR,"gpu_mode != GPU_FORCE is not supported yet!");
}

void PairTBCOMPGPU::cpu_compute(int start, int inum, int eflag, int /* vflag */,
                               int *ilist, int *numneigh, int **firstneigh) {
  int i,j,ii,jj,jnum,itype,jtype;
  double xtmp,ytmp,ztmp,delx,dely,delz,fpair,evdwl;
  double rsq,factor_lj;
  int *jlist,tbi,ptemp;

  double **x = atom->x;
  double **f = atom->f;
  int *type = atom->type;
  double *special_lj = force->special_lj;

  // loop over neighbors of my atoms

  for (ii = start; ii < inum; ii++) {
    i = ilist[ii];
    xtmp = x[i][0];
    ytmp = x[i][1];
    ztmp = x[i][2];
    itype = type[i];
    jlist = firstneigh[i];
    jnum = numneigh[i];

    for (jj = 0; jj < jnum; jj++) {
      j = jlist[jj];
      factor_lj = special_lj[sbmask(j)];
      j &= NEIGHMASK;

      delx = xtmp - x[j][0];
      dely = ytmp - x[j][1];
      delz = ztmp - x[j][2];
      rsq = delx*delx + dely*dely + delz*delz;
      jtype = type[j];

      if (rsq < force_cutoff_sq) {
        rsq=sqrt(rsq);
        tbi=(int)(rsq/ddr);
        if(tbi>=nbin_r) continue;
        fpair = frc[tbi][typ2pair[itype][jtype]]*factor_lj;

        f[i][0] += delx * fpair;
        f[i][1] += dely * fpair;
        f[i][2] += delz * fpair;

        // if (evflag) ev_tally(i, j, nlocal, newton_pair, evdwl, 0.0, fpair, delx, dely, delz);
      }
    }
  }
}