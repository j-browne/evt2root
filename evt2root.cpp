/*
 *
 */
#include <stdio.h>
#include <iostream>
#include <stdlib.h>
#include <getopt.h>
#include <TROOT.h>
#include <TTree.h>
#include <TFile.h>

//#define ADC_PACKET 0xadc1
#define MAX_ADC_CHAN 32
#define ADC_PACKET 104
#define BUFFERSIZE 4096
#define BUFFER_TYPE_DATA 1
#define BUFFER_TYPE_END 12

#define CAEN_GEO_MASK 0xf8000000
#define CAEN_TYPE_MASK 0x07000000
#define CAEN_HEADER_CRATE_MASK 0x00ff0000
#define CAEN_HEADER_CNT_MASK 0x00003f00
#define CAEN_DATA_CHAN_MASK 0x001f0000
#define CAEN_DATA_UN_MASK 0x00002000
#define CAEN_DATA_OV_MASK 0x00001000
#define CAEN_DATA_VAL_MASK 0x00000fff
#define CAEN_EOB_COUNT_MASK 0x00ffffff
#define CAEN_HEADER 0x02000000
#define CAEN_DATA 0x00000000
#define CAEN_EOB 0x04000000

class adc_t
{
public:
	Int_t size;
	Int_t ch[MAX_ADC_CHAN];
	Int_t val[MAX_ADC_CHAN];
	adc_t() {Reset();};
	void Reset(){size=0;for(int i=0; i<MAX_ADC_CHAN; ++i){ch[i]=0;val[i]=0;}}
};

int getArgs(int argc,char **argv);

typedef struct
{
	char filein[100];
	char fileout[100];
} gParameters;

/*
 * Options specified on the command line
 * -i <filename> -- file containing list of input files
 * -o <filename> -- output root file
 */
gParameters gParams={"inputs.dat", "out.root"};

int main (int argc, char *argv[])
{
	FILE *flist; // File containing list of input filenames
	FILE *fp; // Input files
	char line[256];
	int nEvents = 0;

	/* Make sure the args are good. This should never happen */
	if (getArgs(argc,argv)!=0)
	{
		fprintf(stderr,"ERROR: Invalid input!\n");
		exit(-1);
	}

	/* Make sure flist is good */
	if ((flist=fopen(gParams.filein,"r"))==NULL)
	{
		fprintf(stderr,"ERROR: Can't open input file %s !\n",gParams.filein);
		exit(-2);
	}

	adc_t* adc = new adc_t();
	TFile *outfile = new TFile(gParams.fileout,"RECREATE");
	TTree *tree = new TTree("tree","Events");
	tree->Branch("size",&(adc->size),"size/I");
	tree->Branch("chan",adc->ch,"ch[size]/I");
	tree->Branch("val",adc->val,"val[size]/I");

	/* Loop over input files */
	while (fgets(line,256,flist))
	{
		printf("Reading %s", line);
		int strLen = strlen(line);

		if ((int)(line[strLen-1]) == '\n')
		{
			line[strLen-1] = '\0';
		}

		if ((fp=fopen(line,"r"))==NULL)
		{
			fprintf(stderr,"Can't open evtfile %s, moving to next file\n",line);
			continue;
		}

		/* Read in buffers. Buffer structure: http://www.nscl.msu.edu/exp/performingexp/resources/daq#chapter4 */
		unsigned short int buffer[BUFFERSIZE];
		while (fread(buffer, 2, BUFFERSIZE, fp) == BUFFERSIZE && !feof(fp))
		{
			int index = 0;
			/* Buffer Header */
			unsigned short int bufferSize = buffer[index++];
			unsigned short int bufferType = buffer[index++];
			unsigned short int bufferChecksum = buffer[index++];
			unsigned short int runNum = buffer[index++];
			unsigned short int bufferSeq[2] = {buffer[index++],buffer[index++]};
			unsigned short int numEvents = buffer[index++];
			unsigned short int numLAM = buffer[index++];
			unsigned short int numCPUs = buffer[index++];
			unsigned short int numBitReg = buffer[index++];
			unsigned short int bufferRev = buffer[index++];
			index += 5; // Rest of header, unused

			//printf("Begin Buffer: %d\n", bufferType);

			/* Events -- Only if this is a data buffer */
			if (bufferType == BUFFER_TYPE_DATA)
			{
				for (int currEvent = 0; currEvent < numEvents; ++currEvent)
				{
					int eventRead = 1;
					unsigned short int eventSize = buffer[index++];

					//printf("Begin Event: %d, %d\n", currEvent, eventSize);
					++nEvents;
					while (eventRead < eventSize)
					{
						unsigned short int packetSize = buffer[index++];
						unsigned short int packetID = buffer[index++];
						int packetRead = 2;
						//printf("Start Packet: %d, %d\n", packetSize, packetID);

						/* Sanity Checks */
						if (packetSize > BUFFERSIZE)
						{
							fprintf(stderr, "packetSize (%d) > BUFFERSIZE (%d)\n", packetSize, BUFFERSIZE);
							exit(2);
						}
						if (packetSize > eventSize-eventRead)
						{
							printf("packetSize (%d) > eventSize-eventRead (%d). Skipping to next event.\n", packetSize, eventSize-eventRead);
							eventRead += eventSize-eventRead;
							continue;
						}

						/* Here we can have different packets. This is the stuff you want to change. */
						int ind = 0;
						switch(packetID)
						{
						case ADC_PACKET:
							while (packetRead < packetSize)
							{
								unsigned int data;
								unsigned int geo;
								unsigned int type;
								unsigned int crate;
								unsigned int cnt;
								unsigned int chan;
								unsigned int val;
								unsigned int count;
								bool un;
								bool ov;

								/* Each 32-bit word is actually stored as 2 16-bit words reversed for some reason */
								data = buffer[index++]*0x10000;
								data += buffer[index++];
								packetRead += 2;

								/* Caen V785 buffer structure: http://www.caen.it/csite/CaenProd.jsp?parent=11&idmod=37 */
								geo = (data & CAEN_GEO_MASK)>>27;
								type = data & CAEN_TYPE_MASK;
								switch(type)
								{
								case CAEN_HEADER:
									crate = (data& CAEN_HEADER_CRATE_MASK)>>16;
									cnt = (data& CAEN_HEADER_CNT_MASK)>>8;
									adc->size = cnt;
									//printf ("HEADER: GEO: %x CRATE: %x CNT: %x\n", geo, crate, cnt);
									break;
								case CAEN_DATA:
									chan = (data & CAEN_DATA_CHAN_MASK)>>16;
									un = data & CAEN_DATA_UN_MASK;
									ov = data & CAEN_DATA_OV_MASK;
									val = data & CAEN_DATA_VAL_MASK;
									adc->ch[ind++] = chan;
									adc->val[ind++] = val;
									//printf ("DATA: GEO: %x CHAN: %x UN: %x OV: %x VAL: %x\n", geo, chan, un, ov, val);
									break;
								case CAEN_EOB:
									count = data & CAEN_EOB_COUNT_MASK;
									//printf ("EOB: GEO: %x COUNT: %x\n", geo, count);
									break;
								}
							}

							/* Sanity Check */
							if (ind > adc->size)
							{
								printf("Warning: More channels than stated in header.\n");
							}
							break;
						default:
							index += packetSize-2;
							printf("WARNING: Unknown packetID: %x\n", packetID);
							break;
						}

						/* Sanity Check */
						if (packetRead != packetSize)
						{
							fprintf(stderr, "For packetID %x: packetRead (%d) != packetSize (%d)\n", packetID, packetRead, packetSize);
							exit(2);
						}

						eventRead += packetSize;
					}

					if (eventRead != eventSize)
					{
						fprintf(stderr, "eventRead (%d) != eventSize (%d)\n", eventRead, eventSize);
						exit(2);
					}
					tree->Fill();
					adc->Reset();
				}
			}
/*
			// I think this is unneeded
			else if (bufferType == BUFFER_TYPE_END)
			{
				break;
			}
*/
		}
		fclose(fp);
		printf(" Done\n");
	}

	fclose(flist);

	/* Write and close output file */
	tree->Print();
	outfile->Write();
	outfile->Close();

	return 0;
}

int getArgs(int argc, char**argv)
{
	char opt;
	int good = 0;

	while ((opt = (char)getopt(argc, argv, "i:o:")) != EOF)
	{
		switch (opt)
		{
			case 'i': strcpy(gParams.filein,optarg); break;
			case 'o': strcpy(gParams.fileout,optarg); break;
		}
	}

	return (good);
}
