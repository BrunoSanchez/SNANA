/**************************************************
  Created Oct 2019
  Translate fortran kcor-read utilities into C

  Test/debug with 
    snlc_sim.exe <inFile> DEBUG_FLAG 555

  NOT READY !!

***************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <math.h>
#include "fitsio.h"

#include "sntools.h"
#include "sntools_fitsio.h"
#include "sntools_kcor.h" 
#include "sntools_spectrograph.h"
#include "MWgaldust.h"


// ======================================
void READ_KCOR_DRIVER(char *kcorFile, char *FILTERS_SURVEY) {

  char BANNER[100];
  char fnam[] = "READ_KCOR_DRIVER" ;

  // ----------- BEGIN ---------------

  sprintf(BANNER,"%s: read calib/filters/kcor", fnam );
  print_banner(BANNER);

  sprintf(KCOR_INFO.FILENAME, "%s", kcorFile) ;
  sprintf(KCOR_INFO.FILTERS_SURVEY, "%s", FILTERS_SURVEY);
  KCOR_INFO.NFILTDEF_SURVEY = strlen(FILTERS_SURVEY);

  read_kcor_init();

  read_kcor_open();

  read_kcor_head();

  read_kcor_zpoff();

  read_kcor_snsed();

  read_kcor_tables();

  read_kcor_mags();

  read_kcor_filters();

  read_kcor_primarysed();
  //   CALL RDKCOR_SUMMARY(KCORFILE,IERR)


  int istat = 0 ;
  fits_close_file(KCOR_INFO.FP, &istat); 
  debugexit(fnam);

  return ;

} // end READ_KCOR_DRIVER

// ===============================
void read_kcor_init(void) {

  int i, i2;
  char *kcorFile = KCOR_INFO.FILENAME;
  char fnam[] = "read_kcor_init" ;

  // ------------ BEGIN ------------

  if ( IGNOREFILE(kcorFile) ) {
    sprintf(c1err,"Must specifiy kcor/calib file with KCOR_FILE key");
    sprintf(c2err,"KCOR_FILE contains filter trans, primary ref, AB off, etc");
    errmsg(SEV_FATAL, 0, fnam, c1err, c2err);    
  }

  KCOR_INFO.NCALL_READ++ ;
  KCOR_INFO.NKCOR             = 0 ;
  KCOR_INFO.NKCOR_STORE       = 0 ;
  KCOR_INFO.NFILTDEF          = 0 ;
  KCOR_INFO.NPRIMARY          = 0 ;
  KCOR_INFO.NFLAMSHIFT        = 0 ;
  KCOR_INFO.RVMW              = RV_MWDUST ;
  KCOR_INFO.OPT_MWCOLORLAW    = OPT_MWCOLORLAW_ODON94 ;
  KCOR_INFO.STANDALONE        = false ;
  KCOR_INFO.MASK_EXIST_BXFILT = 0 ;

  for(i=0; i < MXFILT_KCOR; i++ ) {
    KCOR_INFO.IFILTDEF[i]   = -9 ;
    KCOR_INFO.ISLAMSHIFT[i] = false;
    KCOR_INFO.MASK_FRAME_FILTER[i] = 0 ;

    for(i2=0; i2 < MXFILT_KCOR; i2++ ) 
      { KCOR_INFO.EXIST_KCOR[i][i2] = false ; }
  }

  for(i=0; i < MXTABLE_KCOR; i++ ) {
    KCOR_INFO.IFILTMAP_KCOR[OPT_FRAME_REST][i] = -9; 
    KCOR_INFO.IFILTMAP_KCOR[OPT_FRAME_OBS][i]  = -9;
  }

  KCOR_VERBOSE_FLAG = 1;

  addFilter_kcor(0, &KCOR_INFO.FILTERMAP_REST); // zero map
  addFilter_kcor(0, &KCOR_INFO.FILTERMAP_OBS ); // zero map

  IFILTDEF_BESS_BX = INTFILTER("X");
 
  return ;

} // end read_kcor_init


// ===============================
void  read_kcor_open(void) {

  int istat=0;
  char kcorFile[MXPATHLEN] ;
  char fnam[] = "read_kcor_open" ;

  // -------- BEGIN --------

  sprintf(kcorFile, "%s", KCOR_INFO.FILENAME);
  fits_open_file(&KCOR_INFO.FP, kcorFile, READONLY, &istat);

  // if kcorFile doesn't exist, try reading from SNDATA_ROOT
  if ( istat != 0 ) {
    istat = 0;
    sprintf(kcorFile,"%s/kcor/%s", 
	    getenv("SNDATA_ROOT"), KCOR_INFO.FILENAME);
    fits_open_file(&KCOR_INFO.FP, kcorFile, READONLY, &istat);
  }

  if ( istat != 0 ) {					       
    sprintf(c1err,"Could not open %s", KCOR_INFO.FILENAME );  
    sprintf(c2err,"Check local dir and $SNDATA_ROOT/kcor");
    errmsg(SEV_FATAL, 0, fnam, c1err, c2err); 
  }

  printf("  Opened %s\n", kcorFile); fflush(stdout);

  return ;

} // end read_kcor_open


// =============================
void read_kcor_head(void) {

  // Read FITS header keys

  fitsfile *FP = KCOR_INFO.FP;
  int istat = 0, i, IFILTDEF, len ;
  char KEYWORD[40], comment[100], tmpStr[100], cfilt[2] ;
  int  NUMPRINT =  14;
  int  MEMC     = 80*sizeof(char); // for malloc

  int *IPTR; double *DPTR; float *FPTR; char *SPTR;
  char fnam[] = "read_kcor_head" ;

  // --------- BEGIN ---------- 
  printf("   %s \n", fnam); fflush(stdout);

  sprintf(KEYWORD,"VERSION");  IPTR = &KCOR_INFO.VERSION;
  fits_read_key(FP, TINT, KEYWORD, IPTR, comment, &istat);
  snfitsio_errorCheck("can't read VERSION", istat);
  printf("\t Read %-*s  = %d  (kcor.exe version) \n", 
	 NUMPRINT, KEYWORD, *IPTR);

  sprintf(KEYWORD,"NPRIM");  IPTR = &KCOR_INFO.NPRIMARY;
  fits_read_key(FP, TINT, KEYWORD, IPTR, comment, &istat);
  snfitsio_errorCheck("can't read NPRIM", istat);
  printf("\t Read %-*s  = %d  primary refs \n", 
	 NUMPRINT, KEYWORD, *IPTR );

  // read name of each primary
  for(i=0; i < KCOR_INFO.NPRIMARY; i++ ) {
    sprintf(KEYWORD,"PRIM%3.3d", i+1); 
    fits_read_key(FP, TSTRING, KEYWORD, tmpStr, comment, &istat);
    // extract first word 
    SPTR = strtok(tmpStr," "); 
    KCOR_INFO.PRIMARY_NAME[i] = (char*)malloc(MEMC);
    sprintf(KCOR_INFO.PRIMARY_NAME[i],"%s",SPTR);

    sprintf(c1err,"can't read %s", KEYWORD);
    snfitsio_errorCheck(c1err, istat);
    printf("\t Read %-*s  = %s \n", NUMPRINT, KEYWORD, SPTR );
  }

  // read NFILTERS
  sprintf(KEYWORD,"NFILTERS");  IPTR = &KCOR_INFO.NFILTDEF;
  fits_read_key(FP, TINT, KEYWORD, IPTR, comment, &istat);
  snfitsio_errorCheck("can't read NFILTERS", istat);
  printf("\t Read %-*s  = %d  filters \n", NUMPRINT, KEYWORD, *IPTR );
  
  // read name of each filter
  for(i=0; i < KCOR_INFO.NFILTDEF; i++ ) {
    sprintf(KEYWORD,"FILT%3.3d", i+1);
    KCOR_INFO.FILTER_NAME[i] = (char*)malloc(MEMC);
    SPTR=KCOR_INFO.FILTER_NAME[i];
    fits_read_key(FP, TSTRING, KEYWORD, SPTR, comment, &istat);

    sprintf(c1err,"can't read %s", KEYWORD);
    snfitsio_errorCheck(c1err, istat);
    // printf("\t Read %-*s  = '%s' \n", NUMPRINT, KEYWORD, SPTR );

    if ( SPTR[0] == '*' ) { KCOR_INFO.ISLAMSHIFT[i] = true; }

    // store absolute filter index 
    len = strlen(KCOR_INFO.FILTER_NAME[i]);
    sprintf(cfilt, "%c", KCOR_INFO.FILTER_NAME[i][len-1] ) ;
    IFILTDEF = INTFILTER(cfilt) ;
    KCOR_INFO.IFILTDEF[i] = IFILTDEF ;
	    
  }  // end NFILTDEF loop

  int NFLAMSHIFT = KCOR_INFO.NFLAMSHIFT ;
  int NFILTDEF   = KCOR_INFO.NFILTDEF ;

  // - - - - - - 

  // check lam-shifted filters
  if ( NFLAMSHIFT > 0 ) {
    if ( NFLAMSHIFT == NFILTDEF/2 ) {
      KCOR_INFO.ISLAMSHIFT[NFILTDEF] = true;
      NFILTDEF = KCOR_INFO.NFILTDEF  = NFLAMSHIFT;
    }
    else {
      sprintf(c1err,"NFLAMSHIFT=%d != (NFILTDEF/2 =%d/2",
	      NFLAMSHIFT, NFILTDEF);
      sprintf(c2err,"Check LAM-shifted bands in kcor file.");
      errmsg(SEV_FATAL, 0, fnam, c1err, c2err);       
    }
  }

  // - - - - - - - - 
  // read optional Galactic RV and color law used to warp spectra for k-corr.
  // Note that there is no abort on missing key.

  sprintf(KEYWORD,"RV");  DPTR = &KCOR_INFO.RVMW ;
  fits_read_key(FP, TDOUBLE, KEYWORD, DPTR, comment, &istat);
  if ( istat == 0 ) 
    { printf("\t Read %-*s  = %4.2f \n",   NUMPRINT, KEYWORD, *DPTR ); }
  istat = 0 ;

  sprintf(KEYWORD,"OPT_MWCOLORLAW");  IPTR = &KCOR_INFO.OPT_MWCOLORLAW ;
  fits_read_key(FP, TINT, KEYWORD, IPTR, comment, &istat);
  if ( istat == 0 ) 
    { printf("\t Read %-*s  = %d \n",   NUMPRINT, KEYWORD, *IPTR ); }
  istat = 0 ;
  
  // read number of KCOR tables
  sprintf(KEYWORD,"NKCOR");  IPTR = &KCOR_INFO.NKCOR ;
  fits_read_key(FP, TINT, KEYWORD, IPTR, comment, &istat);
  snfitsio_errorCheck("can't read NKCOR", istat);
  printf("\t Read %-*s  = %d  K-COR tables \n", NUMPRINT, KEYWORD, *IPTR );
  
  int NKCOR = KCOR_INFO.NKCOR;
  if ( NKCOR > MXTABLE_KCOR ) {
    sprintf(c1err,"NKCOR=%d exceeds bound of MXTABLE_KCOR=%d",
	    NKCOR, MXTABLE_KCOR );
    sprintf(c2err,"Check NKCOR in FITS header");
    errmsg(SEV_FATAL, 0, fnam, c1err, c2err);       
  }

  // kcor strings that define rest- and obs-frame bands
  for(i=0; i < NKCOR; i++ ) {
    sprintf(KEYWORD,"KCOR%3.3d", i+1);
    KCOR_INFO.KCOR_STRING[i] = (char*)malloc(MEMC);
    SPTR = KCOR_INFO.KCOR_STRING[i];
    fits_read_key(FP, TSTRING, KEYWORD, SPTR, comment, &istat);

    sprintf(c1err,"can't read %s", KEYWORD);
    snfitsio_errorCheck(c1err, istat);
    // printf("\t Read %-*s = %s \n", NUMPRINT, KEYWORD, SPTR );
  }
  

  // - - - - - - - 
  // read bin info 

  read_kcor_binInfo("wavelength", "L"  , MXLAMBIN_FILT,
		    &KCOR_INFO.BININFO_LAM ); 
  read_kcor_binInfo("epoch",      "T"  , MXTBIN_KCOR,
		    &KCOR_INFO.BININFO_T ); 
  read_kcor_binInfo("redshift",   "Z"  , MXZBIN_KCOR,
		    &KCOR_INFO.BININFO_z ); 
  read_kcor_binInfo("AV",         "AV" , MXAVBIN_KCOR,
		    &KCOR_INFO.BININFO_AV ); 

  KCOR_INFO.zRANGE_LOOKUP[0] = KCOR_INFO.BININFO_z.RANGE[0] ;
  KCOR_INFO.zRANGE_LOOKUP[1] = KCOR_INFO.BININFO_z.RANGE[1] ;

  // read optional spectrograph information
  int istat1=0, istat2=0, NFSPEC ;
  sprintf(KEYWORD, "SPECTROGRAPH_INSTRUMENT");
  SPTR = KCOR_INFO.SPECTROGRAPH_INSTRUMENT ;
  fits_read_key(FP, TSTRING, KEYWORD, SPTR, comment, &istat1 );
  if ( istat1 == 0 ) { 
    printf("\t Read SPECTROGRAPH INSTRUMENT = %s \n", SPTR); 

    sprintf(KEYWORD, "SPECTROGRAPH_FILTERLIST");
    SPTR = KCOR_INFO.SPECTROGRAPH_FILTERLIST ;
    fits_read_key(FP, TSTRING, KEYWORD, SPTR, comment, &istat2 );
    
    if ( istat2 == 0 ) {
      NFSPEC = strlen(KCOR_INFO.SPECTROGRAPH_FILTERLIST);
      KCOR_INFO.NFILTDEF_SPECTROGRAPH =  NFSPEC ;
      for(i=0; i < NFSPEC; i++ ) {
	sprintf(cfilt,"%c", KCOR_INFO.SPECTROGRAPH_FILTERLIST[i] );
	IFILTDEF = INTFILTER(cfilt) ;
	KCOR_INFO.IFILTDEF_SPECTROGRAPH[i] = IFILTDEF ; 
      }

    } // end istat2
  } // end istat1 

  return ;

} // end read_kcor_head


// ==================================================
void read_kcor_binInfo(char *VARNAME, char *VARSYM, int MXBIN,
		       KCOR_BININFO_DEF *BININFO) {

  // Read the following from header and load BININFO struct with
  //   Number of bins  :  NB[VARSYM]
  //   binsize         :  [VARSYM]BIN
  //   min val         :  [VARSYM]MIN
  //   max val         :  [VARSYM]MAX
  //
  // Abort if number of bins exceeds MXBIN

  fitsfile *FP = KCOR_INFO.FP;
  int istat = 0, i, NBIN ;
  char KEYWORD[20], comment[100];
  int *IPTR; double *DPTR;
  char fnam[] = "read_kcor_binInfo" ;

  // ----------- BEGIN -----------

  sprintf(BININFO->VARNAME, "%s", VARNAME);

  sprintf(KEYWORD,"NB%s", VARSYM);  IPTR = &BININFO->NBIN;
  fits_read_key(FP, TINT, KEYWORD, IPTR, comment, &istat);
  sprintf(c1err,"can't read %s", KEYWORD);
  snfitsio_errorCheck(c1err, istat);

  NBIN = BININFO->NBIN ;

  sprintf(KEYWORD,"%sBIN", VARSYM);  DPTR = &BININFO->BINSIZE;
  fits_read_key(FP, TDOUBLE, KEYWORD, DPTR, comment, &istat);
  sprintf(c1err,"can't read %s", KEYWORD);
  snfitsio_errorCheck(c1err, istat);

  sprintf(KEYWORD,"%sMIN", VARSYM);  DPTR = &BININFO->RANGE[0];
  fits_read_key(FP, TDOUBLE, KEYWORD, DPTR, comment, &istat);
  sprintf(c1err,"can't read %s", KEYWORD);
  snfitsio_errorCheck(c1err, istat);

  sprintf(KEYWORD,"%sMAX", VARSYM);  DPTR = &BININFO->RANGE[1];
  fits_read_key(FP, TDOUBLE, KEYWORD, DPTR, comment, &istat);
  sprintf(c1err,"can't read %s", KEYWORD);
  snfitsio_errorCheck(c1err, istat);

  printf("\t Read %4d %-10s bins (%.2f to %.2f)\n",
	 NBIN, VARNAME, BININFO->RANGE[0], BININFO->RANGE[1]);
  fflush(stdout);


  if ( BININFO->NBIN >= MXBIN ) {
    sprintf(c1err, "NBIN(%s) = %d exceeds bounf of %d",
	    VARNAME, NBIN, MXBIN );
    sprintf(c2err, "Check FITS HEADER keys: NB%s, %sBIN, %sMIN, %sMAX",
	    VARSYM, VARSYM, VARSYM, VARSYM );
    errmsg(SEV_FATAL, 0, fnam, c1err, c2err);       
  }

  // store value in each bin
  double xi, xmin=BININFO->RANGE[0], xbin=BININFO->BINSIZE ;
  BININFO->GRIDVAL = (double*) malloc(NBIN * sizeof(double) );
  for(i=0; i < NBIN; i++ ) {
    xi = (double)i ;
    BININFO->GRIDVAL[i] = xmin + ( xi * xbin ) ;
  }

  return ;

} // end read_kcor_binInfo

// =============================
void read_kcor_zpoff(void) {

  // read filter-dependent info:
  // PrimaryName  PrimaryMag  ZPTOFF(prim)  ZPTOFF(filt)

  fitsfile *FP = KCOR_INFO.FP ;
  int  NFILTDEF = KCOR_INFO.NFILTDEF ;
  int  NPRIMARY = KCOR_INFO.NPRIMARY ; 

  int  hdutype, istat=0, ifilt, anynul, iprim, iprim_store ;
  char **NAME_PRIM, *tmpName ;
  char fnam[] = "read_kcor_zpoff" ;

  int ICOL_FILTER_NAME    = 1 ;
  int ICOL_PRIMARY_NAME   = 2 ;
  int ICOL_PRIMARY_MAG    = 3 ;
  int ICOL_PRIMARY_ZPOFF  = 4 ;
  int ICOL_SNPHOT_ZPOFF   = 5 ;

  long FIRSTROW, NROW, FIRSTELEM = 1;
  
  // --------- BEGIN ----------

  printf("   %s  \n", fnam); fflush(stdout);

  fits_movrel_hdu(FP, 1, &hdutype, &istat);
  snfitsio_errorCheck("Cannot move to ZPOFF table", istat);

  NAME_PRIM = (char**) malloc(2*sizeof(char*) );
  NAME_PRIM[0] = (char*) malloc(60*sizeof(char));
  NAME_PRIM[1] = (char*) malloc(60*sizeof(char));
    
  for(ifilt=0; ifilt < NFILTDEF; ifilt++ ) {
           
    FIRSTROW = ifilt + 1;  NROW=1;
    fits_read_col_str(FP, ICOL_PRIMARY_NAME, FIRSTROW, FIRSTELEM, NROW,
		      NULL_A, NAME_PRIM, &anynul, &istat )  ;      

    // find IPRIM index
    iprim_store = -9;
    for(iprim=0; iprim < NPRIMARY; iprim++ ) {
      tmpName = KCOR_INFO.PRIMARY_NAME[iprim] ;
      if ( strcmp(NAME_PRIM[0],tmpName) == 0 ) { iprim_store = iprim; }      
    }

    if( iprim_store < 0 ) {
      sprintf(c1err,"Unrecognized PRIMARY_NAME = %s", NAME_PRIM[0] );
      sprintf(c2err,"Check ZPT table");
      errmsg(SEV_FATAL, 0, fnam, c1err, c2err);       
    }

    KCOR_INFO.PRIMARY_INDX[ifilt] = iprim_store ;   
  } // end ifilt


  FIRSTROW = 1;  NROW = NFILTDEF ;

  // read primary mag, ZPOFF for primary, and photometry offsets
  // passed from optional ZPOFF.DAT file in $SNDATA_ROOT/filters.

  fits_read_col_dbl(FP, ICOL_PRIMARY_MAG, FIRSTROW, FIRSTELEM, NROW,
		    NULL_1D, KCOR_INFO.PRIMARY_MAG, &anynul, &istat )  ;      
  snfitsio_errorCheck("Read PRIMARY_MAG", istat);

  fits_read_col_dbl(FP, ICOL_PRIMARY_ZPOFF, FIRSTROW, FIRSTELEM, NROW,
		    NULL_1D, KCOR_INFO.PRIMARY_ZPOFF, &anynul, &istat )  ;      
  snfitsio_errorCheck("Read PRIMARY_ZPOFF", istat);

  fits_read_col_dbl(FP, ICOL_SNPHOT_ZPOFF, FIRSTROW, FIRSTELEM, NROW,
		    NULL_1D, KCOR_INFO.SNPHOT_ZPOFF, &anynul, &istat )  ;      
  snfitsio_errorCheck("Read SNPHOT_ZPOFF", istat);

  if ( KCOR_VERBOSE_FLAG == 22 ) {
    printf("\n");
    printf("  %s DUMP: \n\n", fnam);
    printf("                    Prim.   Prim.   Prim.    Filter \n");
    printf("  Filter            name    Mag     ZPTOFF   ZPTOFF \n");
    printf(" ----------------------------------------------------- \n");
    for(ifilt=0; ifilt < NFILTDEF; ifilt++ ) {

      iprim = KCOR_INFO.PRIMARY_INDX[ifilt];
      printf(" %-14s %6s(%d)  %6.3f  %7.4f  %7.4f \n",
	     KCOR_INFO.FILTER_NAME[ifilt],
	     KCOR_INFO.PRIMARY_NAME[iprim], KCOR_INFO.PRIMARY_INDX[ifilt],
	     KCOR_INFO.PRIMARY_MAG[ifilt],
	     KCOR_INFO.PRIMARY_ZPOFF[ifilt],
	     KCOR_INFO.SNPHOT_ZPOFF[ifilt] );
	     
    }
  } // end verbose

  return ;

} // end read_kcor_zpoff

// =============================
void read_kcor_snsed(void) {

  fitsfile *FP  = KCOR_INFO.FP ;
  int  NBL      = KCOR_INFO.BININFO_LAM.NBIN;
  int  NBT      = KCOR_INFO.BININFO_T.NBIN;
  long NROW     = NBL * NBT ;
  int  MEMF     = NROW*sizeof(float);
  long FIRSTROW = 1, FIRSTELEM=1 ;
  int  ICOL=1, istat=0, hdutype, anynul ;
  char fnam[] = "read_kcor_snsed" ;
  // --------- BEGIN ----------
  printf("   %s \n", fnam); fflush(stdout);

  KCOR_INFO.FLUX_SNSED = (float*) malloc(MEMF);

  fits_movrel_hdu(FP, 1, &hdutype, &istat);
  snfitsio_errorCheck("Cannot move to SNSED table", istat);

  fits_read_col_flt(FP, ICOL, FIRSTROW, FIRSTELEM, NROW,
		    NULL_1E, KCOR_INFO.FLUX_SNSED, &anynul, &istat )  ;      
  snfitsio_errorCheck("Read FLUX_SNSED", istat);

  return ;

} // end read_kcor_snsed

// =============================
void read_kcor_tables(void) {

  // Examine K_xy to flag which filters are OBS and REST.
  // If there are no K-cor tables, then any SURVEY filter
  // is defined as an OBS filter. 

  fitsfile *FP  = KCOR_INFO.FP ;
  int NKCOR         = KCOR_INFO.NKCOR;
  int NFILTDEF_KCOR = KCOR_INFO.NFILTDEF;

  int NKCOR_STORE = 0 ;
  int k, hdutype, istat=0, ifilt_rest, ifilt_obs, len ;
  int IFILTDEF, ifilt, LBX0, LBX1;
  char *STRING, strKcor[8], cfilt_rest[40], cfilt_obs[40];
  char cband_rest[2], cband_obs[2];
  char fnam[] = "read_kcor_tables" ;

  // --------- BEGIN ----------

  printf("   %s \n", fnam); fflush(stdout);

  fits_movrel_hdu(FP, 1, &hdutype, &istat);
  snfitsio_errorCheck("Cannot move to KCOR table", istat);

  if ( NKCOR == 0 ) { return; }

  for(k=0; k < NKCOR; k++ ) {

    STRING = KCOR_INFO.KCOR_STRING[k] ;

    parse_KCOR_STRING(STRING, strKcor, cfilt_rest, cfilt_obs);
    ifilt_rest = INTFILTER(cfilt_rest);
    ifilt_obs  = INTFILTER(cfilt_obs);

    // if this IFILT_OBS is not a SURVEY filter, then bail.
    // i.e., ignore K_xy that include extra unused filters.

    len = strlen(cfilt_rest); sprintf(cband_rest,"%c", cfilt_rest[len-1]);
    len = strlen(cfilt_obs ); sprintf(cband_obs, "%c", cfilt_obs[len-1]);
    
    if ( strchr(KCOR_INFO.FILTERS_SURVEY,cband_obs[0]) == NULL ) 
      { continue; }

    KCOR_INFO.EXIST_KCOR[ifilt_rest][ifilt_obs] = true ;
    
    // set mask for rest frame and/or observer frame
    for(ifilt=0; ifilt < NFILTDEF_KCOR; ifilt++ ) {
      IFILTDEF = KCOR_INFO.IFILTDEF[ifilt];

      if ( ifilt_rest == IFILTDEF ) 
	{ KCOR_INFO.MASK_FRAME_FILTER[ifilt] |= MASK_FRAME_REST ; }

      if ( ifilt_obs == IFILTDEF ) 
	{ KCOR_INFO.MASK_FRAME_FILTER[ifilt] |= MASK_FRAME_OBS ; }
    } // end ifilt

    LBX0 = ISBXFILT_KCOR(cfilt_rest);
    if ( LBX0 ) { KCOR_INFO.MASK_EXIST_BXFILT |= MASK_FRAME_REST; }

    LBX1 = ISBXFILT_KCOR(cfilt_obs);
    if ( LBX1 ) { KCOR_INFO.MASK_EXIST_BXFILT |= MASK_FRAME_OBS; }

    /* can't remember purpose of STANDALONE mode ... fix later 
         IF ( RDKCOR_STANDALONE .and. 
     &         IFILTDEF_INVMAP_SURVEY(ifilt_obs) .LE. 0 ) THEN
            NFILTDEF_SURVEY = NFILTDEF_SURVEY + 1
            IFILTDEF_MAP_SURVEY(NFILTDEF_SURVEY) = IFILT_OBS
            IFILTDEF_INVMAP_SURVEY(ifilt_obs)    = NFILTDEF_SURVEY   
         ENDIF
    */

    // abort on undefined filter

    if ( ifilt_rest <= 0 || ifilt_obs <= 0 ) {
      sprintf(c1err,"Undefined %s: IFILTDEF(REST,OBS)=%d,%d",
	      strKcor, ifilt_rest, ifilt_obs);
      sprintf(c2err,"Check filters in Kcor file.");
      errmsg(SEV_FATAL, 0, fnam, c1err, c2err); 
    }

    // define new rest-filter only if not already defined.
    addFilter_kcor(ifilt_rest, &KCOR_INFO.FILTERMAP_REST);
    addFilter_kcor(ifilt_obs,  &KCOR_INFO.FILTERMAP_OBS );

    // ??? if ( EXIST_BXFILT_OBS .and. RDKCOR_STANDALONE ) { continue; }   

    KCOR_INFO.IFILTMAP_KCOR[OPT_FRAME_REST][NKCOR_STORE] = ifilt_rest; 
    KCOR_INFO.IFILTMAP_KCOR[OPT_FRAME_OBS][NKCOR_STORE]  = ifilt_obs ;
    KCOR_INFO.k_index[NKCOR_STORE] = k;
    NKCOR_STORE++; 
    KCOR_INFO.NKCOR_STORE = NKCOR_STORE ;


  } // end k loop over KCOR tables


  // sanity check on number of rest,obs filters
  int NFILT_REST = KCOR_INFO.FILTERMAP_REST.NFILTDEF ;
  int NFILT_OBS  = KCOR_INFO.FILTERMAP_OBS.NFILTDEF ;
  if ( NFILT_REST >= MXFILT_REST_KCOR ) {
    sprintf(c1err,"NFILT_REST = %d exceeds bound of %d.",
	    NFILT_REST, MXFILT_REST_KCOR);
    sprintf(c2err,"Check kcor input file.");
    errmsg(SEV_FATAL, 0, fnam, c1err, c2err); 
  }
  if ( NFILT_OBS >= MXFILT_OBS_KCOR ) {
    sprintf(c1err,"NFILT_OBS = %d exceeds bound of %d.",
	    NFILT_OBS, MXFILT_OBS_KCOR);
    sprintf(c2err,"Check kcor input file.");
    errmsg(SEV_FATAL, 0, fnam, c1err, c2err); 
  }


  // ------------------------------------------------------
  // BX check:
  // loop over defined filters (from header) ; if undefined rest-frame 
  // X filter exists, then add it to the rest-frame list. This allows 
  // Landolt option in fitting program without having to explicitly
  // define a K-correction wit the X filter.
  // Beware to set BX before INIT_KCOR_INDICES !!!

  char *NAME;
  for(ifilt=0; ifilt < NFILTDEF_KCOR ; ifilt++ ) {
    NAME     = KCOR_INFO.FILTER_NAME[ifilt];
    if ( !ISBXFILT_KCOR(NAME) ) { continue; } // ensure 'BX', not BLABLA-X 
    addFilter_kcor(IFILTDEF, &KCOR_INFO.FILTERMAP_REST);
    KCOR_INFO.MASK_EXIST_BXFILT        |= MASK_FRAME_REST ; 
    KCOR_INFO.MASK_FRAME_FILTER[ifilt] |= MASK_FRAME_REST ;
  }


  /* xxxxxxx
  // pass dump flags
  addFilter_kcor(777, &KCOR_INFO.FILTERMAP_REST);
  addFilter_kcor(777, &KCOR_INFO.FILTERMAP_OBS );
  xxxxxx */




  // init multi-dimensional array to store KCOR tables
  init_kcor_indices();


/* .xyz
c =============================
c if user has specified any MAGOBS_SHIFT_PRIMARY, MAGOBS_SHIFT_ZP,
c (and same for REST) for a non-existant filter, then ABORT ...
c 
      CALL  RDKCOR_CHECK_MAGSHIFTS


c Loop again over Kcors that have been flagged for reading/storage.
c i.e,, ignore those for which the obs-frame filter is not defined.

      NROW      = NTBIN_KCOR * NZbin_KCOR * NAVbin_KCOR
      NBIN_TOT  = NROW * NFILTDEF_REST * NFILTDEF_SURVEY
      ISTAT     = 0
      FIRSTROW  = 1
      FIRSTELEM = 1

      R4CRAZY = 9999.9
      DO ibin = 1, NBIN_TOT
        R4KCORTABLE1D(ibin)  = R4CRAZY  ! init to crazy value
      ENDDO

      DO 200 i      = 1, NKCOR_STORE
         ifilt_obs  = IFILT2_RDKCOR(OPT_FILTOBS,i)
         ifilt_rest = IFILT2_RDKCOR(OPT_FILTREST,i)
         ikcor      = IKCOR_RDKCOR(i)
         icol       = ikcor + 3  ! skip T,Z,AV columns

         STRKCOR = KCORINFO_STRING_RDKCOR(ikcor)
         CALL RDKCOR_CHECK_IFILT(ifilt_rest,ifilt_obs,STRKCOR)

c     get sparse filter indices
         ifiltr = IFILTDEF_INVMAP_REST(ifilt_rest)
         ifilto = IFILTDEF_INVMAP_SURVEY(ifilt_obs)

         if ( ifilto .LE. 0 ) then
            c1err = 'Invalid obs-filter for'
            c2err = STRKCOR  
            CALL MADABORT(FNAM,C1ERR,C2ERR)   
         endif

c get starting 1D bin for this KCOR
         IBKCOR(KDIM_ifiltr) = ifiltr - 1
         IBKCOR(KDIM_ifilto) = ifilto - 1
         IBKCOR(KDIM_T)      = 0
         IBKCOR(KDIM_Z)      = 0
         IBKCOR(KDIM_AV)     = 0
         IBIN_FIRST          = GET_1DINDEX(IDMAP_KCOR,NKDIM,IBKCOR)+1
         IBIN_LAST           = IBIN_FIRST + NROW - 1

c read table from fits file.
         CALL FTGCVe(LUN, ICOL, firstrow, firstelem, 
     &      NROW, nullf_rdkcor, 
     &      R4KCORTABLE1D(IBIN_FIRST),     ! return arg
     &      anyf_rdkcor, istat)            ! return arg

         CALL RDKCOR_ABORT(FNAM, STRKCOR, ISTAT) ! error check

c apply user mag-shifts
         DO ibin = IBIN_FIRST, IBIN_LAST
            R4KCORTABLE1D(ibin) 
     &         = R4KCORTABLE1D(ibin) 
     &         - MAGREST_SHIFT_PRIMARY_FILT(ifilt_rest)
     &         + MAGOBS_SHIFT_PRIMARY_FILT(ifilt_obs)
         ENDDO

200   CONTINUE


  */

  return ;

} // end read_kcor_tables

// ===========================
int ISBXFILT_KCOR(char *cfilt) {
  // return true if BX is part of filter name
  if ( strstr(cfilt,"BX") != NULL ) { return(1); }
  return(0);
} // end ISBXFILT_KCOR

// ===========================================
void addFilter_kcor(int ifiltdef, KCOR_FILTERMAP_DEF *MAP ) {

  int ifilt, NF;
  char cfilt1[2] ;
  char fnam[] = "addFilter_kcor" ;

  // --------- BEGIN -----------

  if ( ifiltdef == 0 ) {
    // zero map, then return
    MAP->NFILTDEF = 0;
    MAP->FILTERSTRING[0] =  0 ;
    for(ifilt=0; ifilt < MXFILT_KCOR; ifilt++ ) {
      MAP->IFILTDEF[ifilt]     = -9 ; 
      MAP->IFILTDEF_INV[ifilt] = -9 ;
    }
    return ;
  }


  if ( ifiltdef == 777 ) {
    // dump map, then return
    int IFILTDEF ;
    NF = MAP->NFILTDEF;
    printf("\n");
    printf("\t xxx %s dump: \n", fnam);
    printf("\t xxx FILTERSTRING = '%s' \n", MAP->FILTERSTRING);
    for(ifilt=0; ifilt < NF; ifilt++ ) {
      IFILTDEF = MAP->IFILTDEF[ifilt];
      sprintf(cfilt1, "%c", FILTERSTRING[IFILTDEF] );
      printf("\t xxx IFILTDEF[%2d,%s] = %d \n",
	     ifilt, cfilt1, IFILTDEF); fflush(stdout);
    }
    return ;
  }

  // return if this filter is already defined
  if ( MAP->IFILTDEF_INV[ifiltdef] >= 0 ) { return; }

  NF = MAP->NFILTDEF ;
  sprintf(cfilt1, "%c", FILTERSTRING[ifiltdef] );

  MAP->IFILTDEF_INV[ifiltdef] = NF;
  MAP->IFILTDEF[NF]           = ifiltdef;
  strcat(MAP->FILTERSTRING,cfilt1);
  MAP->NFILTDEF++ ;

  return ;

} // end  addFilter_kcor


// ==============================
void parse_KCOR_STRING(char *STRING, 
		       char *strKcor, char *cfilt_rest, char *cfilt_obs) {

  // Parse input STRING of the form:
  //   Kcor K_XY for rest [filterName]X to obs [filterName]Y
  //
  // and return
  //   strKcor    = 'K_XY'
  //   cfilt_rest = [filterName]X
  //   cfilt_obs  = [filterName]Y
  //

  int NWD, iwd, len ;
  char word[40], cband_rest[2], cband_obs[2];
  char fnam[] = "parse_KCOR_STRING" ;

  // ---------- BEGIN ----------

  strKcor[0] = cfilt_rest[0] = cfilt_obs[0] = 0;

  NWD = store_PARSE_WORDS(MSKOPT_PARSE_WORDS_STRING, STRING);

  for(iwd=0; iwd < NWD-1; iwd++ ) {

    get_PARSE_WORD(0, iwd, word );

    if( word[0] == 'K' && word[1] == '_' ) 
      { sprintf(strKcor, "%s", word); }

    if ( strcmp(word,"rest") == 0 ) 
      { get_PARSE_WORD(0, iwd+1, cfilt_rest); }

    if ( strcmp(word,"obs") == 0 ) 
      { get_PARSE_WORD(0, iwd+1, cfilt_obs); }
  } // end iwd loop

  // - - - - - -
  // sanity tests

  int NERR = 0;
  if ( strlen(strKcor) == 0 ) {
    printf(" ERROR: couldn't find required K_XY in KCOR_STRING\n");
    NERR++ ;  fflush(stdout);
  }

  if ( strlen(cfilt_rest) == 0 ) {
    printf(" ERROR: couldn't find rest-frame filter in KCOR_STRING\n");
    NERR++ ;  fflush(stdout);
  }

  if ( strlen(cfilt_obs) == 0 ) {
    printf(" ERROR: couldn't find obs-frame filter in KCOR_STRING\n");
    NERR++ ;  fflush(stdout);
  }

  cband_rest[0] = strKcor[2]; // rest-frame band char
  cband_obs[0]  = strKcor[3]; // obs-frame band char

  len = strlen(cfilt_rest);
  if ( cband_rest[0] != cfilt_rest[len-1] ) {
    printf(" ERROR: rest band='%c' is not compatible with rest filter='%s'\n",
	   cband_rest, cfilt_rest);
    NERR++ ;  fflush(stdout);
  }

  len = strlen(cfilt_obs);
  if ( cband_obs[0] != cfilt_obs[len-1] ) {
    printf(" ERROR: obs band='%c' is not compatible with obs filter='%s'\n",
	   cband_obs, cfilt_obs );
    NERR++ ;  fflush(stdout);
  }

  if ( NERR > 0 ) {
    sprintf(c1err,"Problem parsing KCOR STRING: ");
    sprintf(c2err,"%s", STRING);
    errmsg(SEV_FATAL, 0, fnam, c1err, c2err); 
  }

  return ;
} // end parse_KCOR_STRING


// ======================================
void init_kcor_indices(void) {

  // Init index maps for KCOR-related tables:
  //   KCOR, AVWARP, LCMAP and MWXT.
  // These tables are multi-dimensional, but we use a
  // 1-d table allocation to save memory. This routine
  // prepares index-mappings from the multi-dimensional
  // space to the 1-d space.

  int  NBIN_T          = KCOR_INFO.BININFO_T.NBIN ;
  int  NBIN_z          = KCOR_INFO.BININFO_z.NBIN ;
  int  NBIN_AV         = KCOR_INFO.BININFO_AV.NBIN ;
  int  NFILTDEF_REST   = KCOR_INFO.FILTERMAP_REST.NFILTDEF ;
  int  NFILTDEF_OBS    = KCOR_INFO.FILTERMAP_OBS.NFILTDEF ;
  int  NFILTDEF_SURVEY = KCOR_INFO.NFILTDEF_SURVEY ;
  int  i;
  char fnam[] = "init_kcor_indices";

  // -------------- BEGIN ----------------

  if ( KCOR_INFO.NKCOR_STORE == 0 ) { return; }

 
  sprintf(KCOR_INFO.MAPINFO_KCOR.NAME, "KCOR");
  KCOR_INFO.MAPINFO_KCOR.IDMAP             = IDMAP_KCOR_TABLE ;
  KCOR_INFO.MAPINFO_KCOR.NDIM              = NKDIM_KCOR;
  KCOR_INFO.MAPINFO_KCOR.NBIN[KDIM_T]      = NBIN_T ;
  KCOR_INFO.MAPINFO_KCOR.NBIN[KDIM_z]      = NBIN_z ;
  KCOR_INFO.MAPINFO_KCOR.NBIN[KDIM_AV]     = NBIN_AV ;
  KCOR_INFO.MAPINFO_KCOR.NBIN[KDIM_IFILTr] = NFILTDEF_REST ;
  KCOR_INFO.MAPINFO_KCOR.NBIN[KDIM_IFILTo] = NFILTDEF_OBS ;
  get_MAPINFO_KCOR("NBINTOT", &KCOR_INFO.MAPINFO_KCOR);

  sprintf(KCOR_INFO.MAPINFO_AVWARP.NAME, "AVWARP");
  KCOR_INFO.MAPINFO_AVWARP.IDMAP             = IDMAP_KCOR_AVWARP ;
  KCOR_INFO.MAPINFO_AVWARP.NDIM              = N4DIM_KCOR;
  KCOR_INFO.MAPINFO_AVWARP.NBIN[0]           = NBIN_T ;
  KCOR_INFO.MAPINFO_AVWARP.NBIN[1]           = MXCBIN_AVWARP ;
  KCOR_INFO.MAPINFO_AVWARP.NBIN[2]           = NFILTDEF_REST ;
  KCOR_INFO.MAPINFO_AVWARP.NBIN[3]           = NFILTDEF_REST ;
  get_MAPINFO_KCOR("NBINTOT", &KCOR_INFO.MAPINFO_AVWARP);

  sprintf(KCOR_INFO.MAPINFO_LCMAG.NAME, "LCMAG");
  KCOR_INFO.MAPINFO_LCMAG.IDMAP             = IDMAP_KCOR_LCMAG ;
  KCOR_INFO.MAPINFO_LCMAG.NDIM              = N4DIM_KCOR;
  KCOR_INFO.MAPINFO_LCMAG.NBIN[0]           = NBIN_T ;
  KCOR_INFO.MAPINFO_LCMAG.NBIN[1]           = NBIN_z ;
  KCOR_INFO.MAPINFO_LCMAG.NBIN[2]           = NBIN_AV ;
  KCOR_INFO.MAPINFO_LCMAG.NBIN[3]           = NFILTDEF_REST ;
  get_MAPINFO_KCOR("NBINTOT", &KCOR_INFO.MAPINFO_LCMAG);
  
  sprintf(KCOR_INFO.MAPINFO_MWXT.NAME, "MWXT");
  KCOR_INFO.MAPINFO_MWXT.IDMAP             = IDMAP_KCOR_MWXT ;
  KCOR_INFO.MAPINFO_MWXT.NDIM              = N4DIM_KCOR;
  KCOR_INFO.MAPINFO_MWXT.NBIN[0]           = NBIN_T ;
  KCOR_INFO.MAPINFO_MWXT.NBIN[1]           = NBIN_z ;
  KCOR_INFO.MAPINFO_MWXT.NBIN[2]           = NBIN_AV ;
  KCOR_INFO.MAPINFO_MWXT.NBIN[3]           = NFILTDEF_SURVEY ;
  get_MAPINFO_KCOR("NBINTOT", &KCOR_INFO.MAPINFO_MWXT);
  

  // clear map IDs since RDKCOR can be called multiple times.
  clear_1DINDEX(IDMAP_KCOR_TABLE);
  clear_1DINDEX(IDMAP_KCOR_AVWARP);
  clear_1DINDEX(IDMAP_KCOR_LCMAG);
  clear_1DINDEX(IDMAP_KCOR_MWXT);

  // init multi-dimensional index maps
  init_1DINDEX(IDMAP_KCOR_TABLE,  NKDIM_KCOR, KCOR_INFO.MAPINFO_KCOR.NBIN);
  init_1DINDEX(IDMAP_KCOR_AVWARP, N4DIM_KCOR, KCOR_INFO.MAPINFO_AVWARP.NBIN);
  init_1DINDEX(IDMAP_KCOR_LCMAG,  N4DIM_KCOR, KCOR_INFO.MAPINFO_LCMAG.NBIN);
  init_1DINDEX(IDMAP_KCOR_MWXT,   N4DIM_KCOR, KCOR_INFO.MAPINFO_MWXT.NBIN);

  return ;

} // end init_kcor_indices

// ===============================================
void get_MAPINFO_KCOR(char *what, KCOR_MAPINFO_DEF *MAPINFO) {
  int i, NDIM, NBIN ;
  char string_NBIN[40];
  // ------------- BEGIN ------------
  if ( strcmp(what,"NBINTOT") == 0 ) {
    NDIM = MAPINFO->NDIM;
    MAPINFO->NBINTOT = 1;
    string_NBIN[0] = 0;
    for(i=0; i < NDIM; i++ ) { 
      NBIN = MAPINFO->NBIN[i] ;
      MAPINFO->NBINTOT *= NBIN ; 
      if ( i == 0 ) 
	{ sprintf(string_NBIN,"%d", NBIN); }
      else
	{ sprintf(string_NBIN,"%s x %d", string_NBIN, NBIN); }
    }
    
    printf("\t NBINMAP(%-6s) = %s = %d \n",
	   MAPINFO->NAME, string_NBIN, MAPINFO->NBINTOT );
    fflush(stdout);
  }

  return ;

} // end get_MAPINFO_KCOR


// =============================
void read_kcor_mags(void) {
  char fnam[] = "read_kcor_mags" ;
  // --------- BEGIN ----------
  printf(" xxx %s: Hello \n", fnam); fflush(stdout);
  return ;
} // end read_kcor_mags

// =============================
void read_kcor_filters(void) {
  char fnam[] = "read_kcor_filters" ;
  // --------- BEGIN ----------
  printf(" xxx %s: Hello \n", fnam); fflush(stdout);
  return ;
} // end read_kcor_filters


// =============================
void read_kcor_primarysed(void) {
  char fnam[] = "read_kcor_primarysed" ;
  // --------- BEGIN ----------
  printf(" xxx %s: Hello \n", fnam); fflush(stdout);
  return ;
} // end read_kcor_primarysed





