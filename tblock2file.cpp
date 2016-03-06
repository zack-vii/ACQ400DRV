/*
 * tblock2file.cpp
 *
 *  Created on: 29 Feb 2016
 *      Author: pgm
 *      ref
 *      /home/pgm/PROJECTS.HOY3/GEOSTORE3/TBLOCK2BURST
 */

/*
 *  block on bq
 *  read the tblock. filter the channels, output to file.
 *  new file every block
 *  new dir
 *  /data/JOB/YYYYMMDD-HH/MM/nnnnn.{dat/txt}
 */
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <assert.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <string>
#include <vector>

#include "popt.h"
#include "local.h"
#include "knobs.h"

#include "File.h"

#include "acq-util.h"

#define NCHAN	4

using namespace std;

namespace G {
	int devnum = 0;
	unsigned int nbuffers;
	unsigned int bufferlen;
	unsigned int nchan = NCHAN;
	int wordsize = 2;
	const char *fmt  = "%Y-%j/%H/%M";
	int *channels;				// index from 1
	int nchan_selected;
	int shr = 4;				// scale 20 bit to 16 bit. Gain possible ..
	bool raw = false;
};

void init_globs(void)
{
	getKnob(0, "/etc/acq400/0/NCHAN", &G::nchan);
	unsigned int data32 = false;
	getKnob(0, "/etc/acq400/0/data32", &data32);
	G::wordsize = data32? sizeof(int): sizeof(short);
	getKnob(G::devnum, "nbuffers",  &G::nbuffers);
	getKnob(G::devnum, "bufferlen", &G::bufferlen);

	G::channels = new int[G::nchan+1];
	memset(G::channels, 0, G::nchan+1*sizeof(int));

	File env("/etc/sysconfig/tblock2burst.ini", "r", File::NOCHECK);
	if (env.fp()){
		char ebuf[128];
		while (fgets(ebuf, 128, env.fp())){
			if (strstr(ebuf, "CH=") == ebuf){
				G::nchan_selected =
					acqMakeChannelRange(G::channels, G::nchan, ebuf+3);
			}
			if (strstr(ebuf, "SHR=") == ebuf){
				G::shr = atoi(ebuf+4);
			}
		}
	}

	if (G::nchan_selected == 0){
		G::nchan_selected = acqMakeChannelRange(
				G::channels, G::nchan, ":");
	}
	fprintf(stderr, "selected %d channels\n", G::nchan_selected);
}

struct poptOption opt_table[] = {
	POPT_AUTOHELP
	POPT_TABLEEND
};

void init(int argc, const char** argv) {
	init_globs();
	poptContext opt_context =
			poptGetContext(argv[0], argc, argv, opt_table, 0);
	int rc;
	bool fill_ramp = false;

	while ( (rc = poptGetNextOpt( opt_context )) >= 0 ){
		switch(rc){
			;
		}
	}
}



class Archiver {
protected:
	FILE *bq;
	string job;
	string jobroot;
	string base;
	string dat;
	string txt;
	int seq;
	char ydhm[32];

	char outbase[128];

	File* aux_file;

	char* makeDateRoot(char ydmh[], int maxstr = 32) {
	        time_t t = time(NULL);
	        struct tm *tmp = localtime(&t);
	        strftime(ydmh, maxstr, G::fmt, tmp);
	}
	void mkdir(char* root){
		snprintf(outbase, 128, "%s/%s", jobroot.data(), root);
		char cmd[128];
		strcpy(cmd, "mkdir -p ");
		strcat(cmd, outbase);
		system(cmd);
	}

	void notify() {
		File fn("/dev/shm/tblock2file", "w");
		fprintf(fn(), "FILE=%s/%06d\n", outbase, seq);
	}

	void createAuxFile() {
		if (aux_file) delete aux_file;

		char auxfn[128];
		snprintf(auxfn, 128, "%s/%06d%s", outbase, seq, txt.data());
		aux_file = new File(auxfn, "w");
	}
	void stashAux(const char* in_fname){
		File fp_sensors(in_fname, "r", File::NOCHECK);
		if (fp_sensors()){
			char buf[256];
			fgets(buf, 256, fp_sensors());
			fputs(buf, aux_file->fp());
		}else{
			fprintf(fp_sensors(), "file \"%s\" not available", in_fname);
		}
	}
public:
	Archiver(string _job) :
		job(_job), jobroot("/data/"), base ("/dev/acq400.0.hb/"), dat(".dat"), txt(".txt"),
		seq(0), aux_file(0)	{
		jobroot = jobroot + job;
		bq = fopen("/dev/acq400.0.bqf", "r");
		if (bq == 0){
			perror("/dev/acq400.0.bqf");
			exit(1);
		}
		makeDateRoot(ydhm);
		mkdir(ydhm);
		createAuxFile();
	}

	int operator() () {
		char bufn[32];

		for(; fgets(bufn, 32, bq); ++seq){
			checkTime();
			string _bufn(chomp(bufn));

			process(base + bufn);
			processAux();
		}

		return 0;
	}

	void checkTime() {
		char ydhm1[32];
		makeDateRoot(ydhm1);
		if (strcmp(ydhm, ydhm1) != 0){
			strcpy(ydhm, ydhm1);
			mkdir(ydhm);
		}
	}
	virtual void process(string bufn) = 0;

	void processAux(void) {
		fprintf(aux_file->fp(), "%06d", seq);
		stashAux("/dev/shm/sensors");
		stashAux("/dev/shm/imu");
		fputs("\n", aux_file->fp());
	}

	static Archiver& create(string _job);
};

template <class T>
class RawArchiver : public Archiver {
	friend class Archiver;
protected:
	RawArchiver(string _job) : Archiver(_job) {
		fprintf(stderr, "RawArchiver<%d>\n", sizeof(T));
	}
public:
	virtual void process(string bufn){
		Mapping<T> m(bufn, G::bufferlen);
		char fname[128];
		snprintf(fname, 128, "%s/%06d%s", outbase, seq, dat.data());
		File fout(fname, "w");
		fwrite(m(), sizeof(T), G::bufferlen/sizeof(T), fout());
	}
};

template <class FROM, class TO>
class ChannelArchiver : public Archiver {
	friend class Archiver;
protected:
	TO *tobuf;

	ChannelArchiver(string _job) : Archiver(_job) {
		fprintf(stderr, "ChannelArchiver<%d,%d>\n", sizeof(FROM), sizeof(TO));
		tobuf = new TO [G::nchan_selected];
	}
public:
	virtual ~ChannelArchiver() {
		delete [] tobuf;
	}
	virtual void process(string bufn){
		Mapping<FROM> m(bufn, G::bufferlen);
		char fname[128];
		snprintf(fname, 128, "%s/%06d%s", outbase, seq, dat.data());
		File fout(fname, "w");

		const FROM* cmax = m() + G::bufferlen/sizeof(FROM);

		for (const FROM* cursor = m(); cursor < cmax; cursor += G::nchan){
			TO* toc = tobuf;
			for (int ic = 0;
				ic < G::nchan && toc-tobuf < G::nchan_selected; ++ic){
				if (G::channels[ic+1]){
					*toc++ = (cursor[ic] >> G::shr) & 0x0000FFFF;
				}
			}
		}
		fwrite(tobuf, sizeof(TO), G::nchan_selected, fout());
	}
};
Archiver& Archiver::create(string _job) {
	if (G::raw){
		return * new RawArchiver<int> (_job);
	}else{
		return * new ChannelArchiver<int, short> (_job);
	}
}

int main(int argc, const char** argv)
{
	init(argc, argv);
	Archiver& archiver = Archiver::create("job");
	return archiver();
}

