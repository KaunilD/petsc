#ifndef lint
static char vcid[] = "$Id: newls1.c,v 1.1 1995/03/20 22:59:54 bsmith Exp bsmith $";
#endif

#include <math.h>
#include "newls1.h"

int SNESNewtonDefaultMonitor(SNES,int,Vec,Vec, double ,void*);
int SNESNewtonDefaultConverged(SNES, double ,double ,double,void * );

/*
     Implements a line search variant of Newton's Method 
    for solving systems of nonlinear equations.  

    Input parameters:
.   snes - nonlinear context obtained from SNESCreate()

    Output Parameters:
.   its  - Number of global iterations until termination.

    Notes:
    See SNESCreate() and SNESSetUp() for information on the definition and
    initialization of the nonlinear solver context.  

    This implements essentially a truncated Newton method with a
    line search.  By default a cubic backtracking line search 
    is employed, as described in the text "Numerical Methods for
    Unconstrained Optimization and Nonlinear Equations" by Dennis 
    and Schnabel.  See the examples in src/snes/examples.
*/

int SNESSolve_LS( SNES snes, int *outits )
{
  SNES_LS *neP = (SNES_LS *) snes->data;
  int     maxits, i, iters, line, nlconv, history_len,ierr,lits;
  double  fnorm, gnorm, gpnorm, xnorm, ynorm, *history;
  Vec     Y, X, F, G, W, TMP;

  history	= snes->conv_hist;	/* convergence history */
  history_len	= snes->conv_hist_len;	/* convergence history length */
  maxits	= snes->max_its;	/* maximum number of iterations */
  X		= snes->vec_sol;		/* solution vector */
  F		= snes->vec_res;		/* residual vector */
  Y		= snes->work[0];		/* work vectors */
  G		= snes->work[1];
  W		= snes->work[2];

  ierr = SNESComputeInitialGuess(snes,X); CHKERR(ierr);  /* X <- X_0 */
  VecNorm( X, &xnorm );		       /* xnorm = || X || */
  ierr = SNESComputeResidual(snes,X,F); CHKERR(ierr); /* (+/-) F(X) */
  VecNorm(F, &fnorm );	        	/* fnorm <- || F || */  
  snes->norm = fnorm;
  if (history && history_len > 0) history[0] = fnorm;
  (*snes->Monitor)(snes,0,X,F,fnorm,snes->monP);  /* Monitor progress */
        
  for ( i=0; i<maxits; i++ ) {
       snes->iter = i+1;

       /* Solve J Y = -F, where J is Jacobian matrix */
       (*snes->ComputeJacobian)(X,&snes->jacobian,snes->jacP);
       ierr = SLESSetOperators(snes->sles,snes->jacobian,snes->jacobian,0);
       ierr = SLESSolve(snes->sles,F,Y,&lits); CHKERR(ierr);
       ierr = (*neP->LineSearch)(snes, X, F, G, Y, W, fnorm, &ynorm, &gnorm );

       TMP = F; F = G; G = TMP;
       TMP = X; X = Y; Y = TMP;
       fnorm = gnorm;

       snes->norm = fnorm;
       if (history && history_len > i+1) history[i+1] = fnorm;
       VecNorm( X, &xnorm );		/* xnorm = || X || */
       (*snes->Monitor)(snes,i+1,X,F,fnorm,snes->monP);  /* Monitor progress */

       /* Test for convergence */
       if ((*snes->Converged)( snes, xnorm, ynorm, fnorm,snes->cnvP )) {
           if (X != snes->vec_sol) VecCopy( X, snes->vec_sol );
           break;
       }
  }
  if (i == maxits) i--;
  *outits = i+1;
  return 0;
}
/* ------------------------------------------------------------ */
/*ARGSUSED*/
int SNESSetUp_LS(SNES snes )
{
  SNES_LS *ctx = (SNES_LS *)snes->data;
  int             ierr;
  snes->nwork = 3;
  ierr = VecGetVecs( snes->vec_sol, snes->nwork,&snes->work ); CHKERR(ierr);
  PLogObjectParents(snes,snes->nwork,snes->work ); 
  return 0;
}
/* ------------------------------------------------------------ */
/*ARGSUSED*/
int SNESDestroy_LS(PetscObject obj)
{
  SNES snes = (SNES) obj;
  SLESDestroy(snes->sles);
  VecFreeVecs(snes->work, snes->nwork );
  PLogObjectDestroy(obj);
  PETSCHEADERDESTROY(obj);
  return 0;
}
/*@ 
   SNESDefaultMonitor - Default monitor for NLE solvers.  Prints the 
   residual norm at each iteration.

   Input Parameters:
.  nlP - nonlinear context
.  x - current iterate
.  f - current residual (+/-)
.  fnorm - 2-norm residual value (may be estimated).

   Notes:
   f is either the residual or its negative, depending on the user's
   preference, as set with NLSetResidualRoutine().


@*/
int SNESDefaultMonitor(SNES snes,int its, Vec x,Vec f,double fnorm,void *dummy)
{
  fprintf( stdout, "iter = %d, residual norm %g \n",its,fnorm);
  return 0;
}

/*@ 
   SNESDefaultConverged - Default test for monitoring the convergence 
   of the NLE solvers.

   Input Parameters:
.  nlP - nonlinear context
.  xnorm - 2-norm of current iterate
.  pnorm - 2-norm of current step 
.  fnorm - 2-norm of residual

   Returns:
$  2  if  ( fnorm < atol ),
$  3  if  ( pnorm < xtol*xnorm ),
$ -2  if  ( nres > max_res ),
$  0  otherwise,

   where
$    atol    - absolute residual norm tolerance,
$              set with NLSetAbsConvergenceTol()
$    max_res - maximum number of residual evaluations,
$              set with NLSetMaxResidualEvaluations()
$    nres    - number of residual evaluations
$    xtol    - relative residual norm tolerance,

$              set with NLSetMaxResidualEvaluations()
$    nres    - number of residual evaluations
$    xtol    - relative residual norm tolerance,
$              set with NLSetRelConvergenceTol()

@*/
int SNESDefaultConverged(SNES snes,double xnorm,double pnorm,double fnorm,
                         void *dummy)
{
  if (fnorm < snes->atol) {
    PLogInfo((PetscObject)snes,"Converged due to absolute residual norm %g < %g\n",
                   fnorm,snes->atol);
    return 2;
  }
  if (pnorm < snes->xtol*(xnorm)) {
    PLogInfo((PetscObject)snes,"Converged due to small update length  %g < %g*%g\n",
                   pnorm,snes->xtol,xnorm);
    return 3;
  }
  return 0;
}

/* ------------------------------------------------------------ */
/*ARGSUSED*/
/*@
   SNESNoLineSearch - This routine is not a line search at all; 
   it simply uses the full Newton step.  Thus, this routine is intended 
   to serve as a template and is not recommended for general use.  

   Input Parameters:
.  snes - nonlinear context
.  x - current iterate
.  f - residual evaluated at x
.  y - search direction (contains new iterate on output)
.  w - work vector
.  fnorm - 2-norm of f

   Output parameters:
.  g - residual evaluated at new iterate y
.  y - new iterate (contains search direction on input)
.  gnorm - 2-norm of g
.  ynorm - 2-norm of search length

   Returns:
   1, indicating success of the step.

@*/
int SNESNoLineSearch(SNES snes, Vec x, Vec f, Vec g, Vec y, Vec w,
                             double fnorm, double *ynorm, double *gnorm )
{
  int    ierr;
  Scalar one = 1.0;
  VecNorm(y, ynorm );	/* ynorm = || y ||    */
  VecAXPY(&one, x, y );	/* y <- x + y         */
  ierr = SNESComputeResidual(snes,y,g); CHKERR(ierr);
  VecNorm( g, gnorm ); 	/* gnorm = || g ||    */
  return 1;
}
/* ------------------------------------------------------------------ */
/*@
   SNESCubicLineSearch - This routine performs a cubic line search.

   Input Parameters:
.  snes - nonlinear context
.  x - current iterate
.  f - residual evaluated at x
.  y - search direction (contains new iterate on output)
.  w - work vector
.  fnorm - 2-norm of f

   Output parameters:
.  g - residual evaluated at new iterate y
.  y - new iterate (contains search direction on input)
.  gnorm - 2-norm of g
.  ynorm - 2-norm of search length

   Returns:
   1 if the line search succeeds; 0 if the line search fails.

   Notes:
   Use either NLSetStepLineSearchRoutines() or NLSetLineSearchRoutine()
   to set this routine within the SNES_NLS1 method.  

   This line search is taken from "Numerical Methods for Unconstrained 
   Optimization and Nonlinear Equations" by Dennis and Schnabel, page 325.

@*/
int SNESCubicLineSearch(SNES snes, Vec x, Vec f, Vec g, Vec y, Vec w,
                              double fnorm, double *ynorm, double *gnorm )
{
  double  steptol, initslope;
  double  lambdaprev, gnormprev;
  double  a, b, d, t1, t2;
  Scalar  cinitslope,clambda;
  int     ierr,count;
  SNES_LS *neP = (SNES_LS *) snes->data;
  Scalar  one = 1.0,scale;
  double  maxstep,minlambda,alpha,lambda,lambdatemp;

  alpha   = neP->alpha;
  maxstep = neP->maxstep;
  steptol = neP->steptol;

  VecNorm(y, ynorm );
  if (*ynorm > maxstep) {	/* Step too big, so scale back */
    scale = maxstep/(*ynorm);
    PLogInfo((PetscObject)snes,"Scaling step by %g\n",scale);
    VecScale(&scale, y ); 
    *ynorm = maxstep;
  }
  minlambda = steptol/(*ynorm);
#if defined(PETSC_COMPLEX)
  VecDot(f, y, &cinitslope ); 
  initslope = real(cinitslope);
#else
  VecDot(f, y, &initslope ); 
#endif
  if (initslope > 0.0) initslope = -initslope;
  if (initslope == 0.0) initslope = -1.0;

  VecCopy(y, w );
  VecAXPY(&one, x, w );
  ierr = SNESComputeResidual(snes,w,g); CHKERR(ierr);
  VecNorm(g, gnorm ); 
  if (*gnorm <= fnorm + alpha*initslope) {	/* Sufficient reduction */
      VecCopy(w, y );
      PLogInfo((PetscObject)snes,"Using full step\n");
      return 0;
  }

  /* Fit points with quadratic */
  lambda = 1.0; count = 0;
  lambdatemp = -initslope/(2.0*(*gnorm - fnorm - initslope));
  lambdaprev = lambda;
  gnormprev = *gnorm;
  if (lambdatemp <= .1*lambda) { 
      lambda = .1*lambda; 
  } else lambda = lambdatemp;
  VecCopy(x, w );
#if defined(PETSC_COMPLEX)
  clambda = lambda; VecAXPY(&clambda, y, w );
#else
  VecAXPY(&lambda, y, w );
#endif
  ierr = SNESComputeResidual(snes,w,g); CHKERR(ierr);
  VecNorm(g, gnorm ); 
  if (*gnorm <= fnorm + alpha*initslope) {      /* sufficient reduction */
      VecCopy(w, y );
      PLogInfo((PetscObject)snes,"Quadratically determined step, lambda %g\n",lambda);
      return 0;
  }

  /* Fit points with cubic */
  count = 1;
  while (1) {
      if (lambda <= minlambda) { /* bad luck; use full step */
           PLogInfo((PetscObject)snes,"Unable to find good step length! %d \n",count);
           PLogInfo((PetscObject)snes, "f %g fnew %g ynorm %g lambda %g \n",
                   fnorm,*gnorm, *ynorm,lambda);
           VecCopy(w, y );
           return 0;
      }
      t1 = *gnorm - fnorm - lambda*initslope;
      t2 = gnormprev  - fnorm - lambdaprev*initslope;
      a = (t1/(lambda*lambda) - 
                t2/(lambdaprev*lambdaprev))/(lambda-lambdaprev);
      b = (-lambdaprev*t1/(lambda*lambda) + 
                lambda*t2/(lambdaprev*lambdaprev))/(lambda-lambdaprev);
      d = b*b - 3*a*initslope;
      if (d < 0.0) d = 0.0;
      if (a == 0.0) {
         lambdatemp = -initslope/(2.0*b);
      } else {
         lambdatemp = (-b + sqrt(d))/(3.0*a);
      }
      if (lambdatemp > .5*lambda) {
         lambdatemp = .5*lambda;
      }
      lambdaprev = lambda;
      gnormprev = *gnorm;
      if (lambdatemp <= .1*lambda) {
         lambda = .1*lambda;
      }
      else lambda = lambdatemp;
      VecCopy( x, w );
#if defined(PETSC_COMPLEX)
      VecAXPY(&clambda, y, w );
#else
      VecAXPY(&lambda, y, w );
#endif
      ierr = SNESComputeResidual(snes,w,g); CHKERR(ierr);
      VecNorm(g, gnorm ); 
      if (*gnorm <= fnorm + alpha*initslope) {      /* is reduction enough */
         VecCopy(w, y );
         PLogInfo((PetscObject)snes,"Cubically determined step, lambda %g\n",lambda);
         return 0;
      }
      count++;
   }
  return 0;
}
/*@
   SNESQuadraticLineSearch - This routine performs a cubic line search.

   Input Parameters:
.  snes - nonlinear context
.  x - current iterate
.  f - residual evaluated at x
.  y - search direction (contains new iterate on output)
.  w - work vector
.  fnorm - 2-norm of f

   Output parameters:
.  g - residual evaluated at new iterate y
.  y - new iterate (contains search direction on input)
.  gnorm - 2-norm of g
.  ynorm - 2-norm of search length

   Returns:
   1 if the line search succeeds; 0 if the line search fails.

   Notes:
   Use SNESSetLineSearchRoutines()
   to set this routine within the SNES_NLS1 method.  

@*/
int SNESQuadraticLineSearch(SNES snes, Vec x, Vec f, Vec g, Vec y, Vec w,
                              double fnorm, double *ynorm, double *gnorm )
{
  double  steptol, initslope;
  double  lambdaprev, gnormprev;
  double  a, b, d, t1, t2;
  Scalar  cinitslope,clambda;
  int     ierr,count;
  SNES_LS *neP = (SNES_LS *) snes->data;
  Scalar  one = 1.0,scale;
  double  maxstep,minlambda,alpha,lambda,lambdatemp;

  alpha   = neP->alpha;
  maxstep = neP->maxstep;
  steptol = neP->steptol;

  VecNorm(y, ynorm );
  if (*ynorm > maxstep) {	/* Step too big, so scale back */
    scale = maxstep/(*ynorm);
    VecScale(&scale, y ); 
    *ynorm = maxstep;
  }
  minlambda = steptol/(*ynorm);
#if defined(PETSC_COMPLEX)
  VecDot(f, y, &cinitslope ); 
  initslope = real(cinitslope);
#else
  VecDot(f, y, &initslope ); 
#endif
  if (initslope > 0.0) initslope = -initslope;
  if (initslope == 0.0) initslope = -1.0;

  VecCopy(y, w );
  VecAXPY(&one, x, w );
  ierr = SNESComputeResidual(snes,w,g); CHKERR(ierr);
  VecNorm(g, gnorm ); 
  if (*gnorm <= fnorm + alpha*initslope) {	/* Sufficient reduction */
      VecCopy(w, y );
      PLogInfo((PetscObject)snes,"Using full step\n");
      return 0;
  }

  /* Fit points with quadratic */
  lambda = 1.0; count = 0;
  count = 1;
  while (1) {
    if (lambda <= minlambda) { /* bad luck; use full step */
      PLogInfo((PetscObject)snes,"Unable to find good step length! %d \n",count);
      PLogInfo((PetscObject)snes, "f %g fnew %g ynorm %g lambda %g \n",
                   fnorm,*gnorm, *ynorm,lambda);
      VecCopy(w, y );
      return 0;
    }
    lambdatemp = -initslope/(2.0*(*gnorm - fnorm - initslope));
    lambdaprev = lambda;
    gnormprev = *gnorm;
    if (lambdatemp <= .1*lambda) { 
      lambda = .1*lambda; 
    } else lambda = lambdatemp;
    VecCopy(x, w );
#if defined(PETSC_COMPLEX)
    clambda = lambda; VecAXPY(&clambda, y, w );
#else
    VecAXPY(&lambda, y, w );
#endif
    ierr = SNESComputeResidual(snes,w,g); CHKERR(ierr);
    VecNorm(g, gnorm ); 
    if (*gnorm <= fnorm + alpha*initslope) {      /* sufficient reduction */
      VecCopy(w, y );
      PLogInfo((PetscObject)snes,"Quadratically determined step, lambda %g\n",lambda);
      return 0;
    }
    count++;
  }

  return 0;
}
/* ------------------------------------------------------------ */
/*@C
   SNESSetLineSearchRoutine - Sets the line search routine to be used
   by the method SNES_NLS1.

   Input Parameters:
.  snes - nonlinear context obtained from NLCreate()
.  func - pointer to int function

   Possible routines:
   SNESCubicLineSearch() - default line search
   SNESNoLineSearch() - the full Newton step (actually not a line search)

   Calling sequence of func:
.  func (SNES snes, Vec x, Vec f, Vec g, Vec y,
         Vec w, double fnorm, double *ynorm, double *gnorm )

    Input parameters for func:
.   snes - nonlinear context
.   x - current iterate
.   f - residual evaluated at x
.   y - search direction (contains new iterate on output)
.   w - work vector
.   fnorm - 2-norm of f

    Output parameters for func:
.   g - residual evaluated at new iterate y
.   y - new iterate (contains search direction on input)
.   gnorm - 2-norm of g
.   ynorm - 2-norm of search length

    Returned by func:
    1 if the line search succeeds; 0 if the line search fails.
@*/
int SNESSetLineSearchRoutine(SNES snes,int (*func)(SNES,Vec,Vec,Vec,Vec,Vec,
                             double,double *,double*) )
{
  if ((snes)->type == SNES_NLS1)
    ((SNES_LS *)(snes->data))->LineSearch = func;
  return 0;
}

static int SNESPrintHelp_LS(SNES snes)
{
  SNES_LS *ls = (SNES_LS *)snes->data;
  fprintf(stderr,"-snes_line_search [basic,quadratic,cubic]\n");
  fprintf(stderr,"-snes_line_search_alpha alpha (default %g)\n",ls->alpha);
  fprintf(stderr,"-snes_line_search_maxstep max (default %g)\n",ls->maxstep);
  fprintf(stderr,"-snes_line_search_steptol tol (default %g)\n",ls->steptol);
}

static int SNESSetFromOptions_LS(SNES snes)
{
  SNES_LS *ls = (SNES_LS *)snes->data;
  char    ver[16];
  double  tmp;

  if (OptionsGetDouble(0,snes->prefix,"-snes_line_search_alpa",&tmp)) {
    ls->alpha = tmp;
  }
  if (OptionsGetDouble(0,snes->prefix,"-snes_line_search_maxstep",&tmp)) {
    ls->maxstep = tmp;
  }
  if (OptionsGetDouble(0,snes->prefix,"-snes_line_search_steptol",&tmp)) {
    ls->steptol = tmp;
  }
  if (OptionsGetString(0,snes->prefix,"-snes_line_search",ver,16)) {
    if (!strcmp(ver,"basic")) {
      SNESSetLineSearchRoutine(snes,SNESNoLineSearch);
    }
    else if (!strcmp(ver,"quadratic")) {
      SNESSetLineSearchRoutine(snes,SNESQuadraticLineSearch);
    }
    else if (!strcmp(ver,"cubic")) {
      SNESSetLineSearchRoutine(snes,SNESCubicLineSearch);
    }
    else {SETERR(1,"Unknown line search?");}
  }
  return 0;
}

/* ------------------------------------------------------------ */
int SNESCreate_LS(SNES  snes )
{
  SNES_LS *neP;

  snes->type		= SNES_NLS1;
  snes->Setup		= SNESSetUp_LS;
  snes->Solver		= SNESSolve_LS;
  snes->destroy		= SNESDestroy_LS;
  snes->Monitor  	= SNESDefaultMonitor;
  snes->Converged	= SNESDefaultConverged;
  snes->PrintHelp       = SNESPrintHelp_LS;
  snes->SetFromOptions  = SNESSetFromOptions_LS;

  neP			= NEW(SNES_LS);   CHKPTR(neP);
  snes->data    	= (void *) neP;
  neP->alpha		= 1.e-4;
  neP->maxstep		= 1.e8;
  neP->steptol		= 1.e-12;
  neP->LineSearch       = SNESCubicLineSearch;
  return 0;
}




