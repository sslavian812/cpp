/*

		FAT Reader 0.05
		by Mikhail Melnik

*/

#include <windows.h>
#include <iostream>
#include <cstdio>
#include <vector>
#include <map>
#include <queue>
#include <cstring>
#include <algorithm>
#include <string>

using namespace std;

// Getting OS type
#if (defined(linux) || defined(__linux) || defined(__linux__) || defined(__GNU__) || defined(__GLIBC__)) && !defined(_CRAYC)
#define LINUX
#elif defined(_WIN32) || defined(__WIN32__) || defined(WIN32)
#define WINDOWS
#endif

int fseek64(FILE *f, long long offset, int origin)
{
#if defined(LINUX)
  return fseeko64(f, offset, origin);
#elif defined(WINDOWS)
  return _fseeki64(f, offset, origin);
#endif
}

long long ftell64(FILE *f)
{
#if defined(LINUX)
  return ftello64(f);
#elif defined(WINDOWS)
  return _ftelli64(f);
#endif
}

enum FATTypes {FAT16, FAT32};
FATTypes FATType;

FILE *f;
char *FILENAME;
unsigned int BPB_BytsPerSec, BPB_SecPerClus, BPB_RsvdSecCnt;
unsigned int BPB_FATSz16, BPB_FATSz32, FATSz, BPB_RootEntCnt, BPB_TotSec16;
unsigned int RootDirSectors, BPB_TotSec32, TotSec, BPB_NumFATs;
unsigned int FirstDataSector, DataSec, CountofClusters;
unsigned int ThisFATEntOffset, ThisFATSecNum, BPB_RootClus;
unsigned int RootDirSecNum, CurDirSecNum, CurDirFirstCluster;
vector <unsigned int> links;

unsigned int FirstSectorofCluster(unsigned int ClusterNum) {
    if(ClusterNum < 2)
        return RootDirSecNum;
    return ((ClusterNum - 2) * BPB_SecPerClus) + FirstDataSector;
}

unsigned int ClusterofSector(unsigned int SectorNum) {
	if(SectorNum < FirstDataSector)
		return 1;
	else
	    return ((SectorNum - FirstDataSector) / BPB_SecPerClus) + 2;
}

unsigned int FindNextClus(unsigned int i) {
    if(FATType == FAT32) {
        if(links[i] == 0x0FFFFFF7)
            return 0xFFFFFFF7;
        else if(links[i] >= 0x0FFFFFF8)
            return 0xFFFFFFFF;
        else
            return links[i];
    }
    else {
        if(links[i] == 0xFFF7)
            return 0xFFFFFFF7;
        else if(links[i] >= 0xFFF8)
            return 0xFFFFFFFF;
        else
            return links[i];
    }
}

void GetFatTable() {
    fseek64(f, BPB_RsvdSecCnt*BPB_BytsPerSec, SEEK_SET);
    int bytsPerSec;

    if(FATType == FAT32)
        bytsPerSec = 4;
    else
        bytsPerSec = 2;
    links.resize(FATSz*BPB_BytsPerSec/bytsPerSec);
    for(unsigned int i = 0; i < FATSz*BPB_BytsPerSec/bytsPerSec; ++i) {
        fread(&links[i], bytsPerSec, 1, f);
        if(FATType == FAT32)
            links[i] = links[i] & 0x0FFFFFFF;
    }
}

struct file {
    int Num;
    unsigned char ShortName[16];
    wstring LongName, id;
    bool Dir;
    unsigned int Size;
    unsigned int FirstClus;
};

unsigned char ChkSum (unsigned char *pFcbName) {
    unsigned char Sum;
    Sum = 0;
    for (short FcbNameLen=11; FcbNameLen!=0; FcbNameLen--) {
        Sum = ((Sum & 1) ? 0x80 : 0) + (Sum >> 1) + *pFcbName++;
    }
    return (Sum);
}

bool cmp_id(const file &a, const file &b) {
    return (a.Dir && !b.Dir) || (((a.Dir && b.Dir) || (!a.Dir && !b.Dir)) && a.id < b.id);
}

vector <file> ScanFolder(unsigned int DirSecNum, unsigned int ClusterNum) {
    fseek64(f, DirSecNum*BPB_BytsPerSec, SEEK_SET);
    vector <file> F;

    unsigned char buf[128];
    unsigned int secpassed = 0;
    while(true) {
    	if(ClusterNum == 1) {
			if(secpassed == RootDirSectors)
	    		break;
    	}
        else if(secpassed == BPB_SecPerClus*BPB_BytsPerSec/32) {
            ClusterNum = FindNextClus(ClusterNum);
            if(ClusterNum == 0xFFFFFFFF)
                break;
            else if(ClusterNum == 0xFFFFFFF7) {
                cout << "Bad cluster found!" << endl;
                return vector <file> (0);
            }
            else {
                DirSecNum = FirstSectorofCluster(ClusterNum);
                fseek64(f, DirSecNum*BPB_BytsPerSec, SEEK_SET);
                secpassed = 0;
            }
        }
        fread(buf, 32, 1, f);
        secpassed++;
        file newfile;
        newfile.Num = F.size();
        if(buf[11] == 0x0F && buf[0] != 0xE5) {
            //Long entry found
            int kol = buf[0] ^ 0x40;
            vector <unsigned char *> s(kol+1);
            s[0] = new unsigned char[33];
            memcpy(s[0], buf, 32);
            for(int i = 1; i <= kol; ++i) {
                s[i] = new unsigned char[33];
                fread(s[i], 32, 1, f);
				secpassed++;
            }
            memcpy(newfile.ShortName, s[kol], 11);
            reverse(s.begin(), s.end());
            bool failed = false;
            unsigned char chksum = ChkSum(s[0]);
            for(int i = 1; i <= kol; ++i) {
                bool end = false;
                for(int j = 1; j < 11 && !end; j += 2)
                    if(s[i][j] != 0)
                        newfile.LongName += s[i][j] + 256*s[i][j+1];
                    else
                        end = true;
                for(int j = 14; j < 26 && !end; j += 2)
                    if(s[i][j] != 0)
                        newfile.LongName += s[i][j] + 256*s[i][j+1];
                    else
                        end = true;
                for(int j = 28; j < 32 && !end; j += 2)
                    if(s[i][j] != 0)
                        newfile.LongName += s[i][j] + 256*s[i][j+1];
                    else
                        end = true;

                if(s[i][13] != chksum) {
                    failed = true;
                    break;
                }
            }
            if(failed)
                continue;
            newfile.Size = (((((s[0][31] << 8) + s[0][30]) << 8) + s[0][29]) << 8) + s[0][28];
            newfile.FirstClus = (((((s[0][21] << 8) + s[0][20]) << 8) + s[0][27]) << 8) + s[0][26];
            if((s[0][11] & 0x18) == 0x00) {
                //File found
                newfile.Dir = false;
            }
            else if((s[0][11] & 0x18) == 0x08) {
                //Root dir found
                continue;
            }
            else if((s[0][11] & 0x18) == 0x10) {
                //Dir found
                newfile.Dir = true;
            }
            else {
                //Invalid dir
                continue;
            }
        }
        else if(buf[0] == 0x00) {
            //End of dir
			continue;
        }
        else if(buf[0] == 0x05 || buf[0] == 0xE5) {
            //Empty element
            continue;
        }
        else {
            memcpy(newfile.ShortName, buf, 11);
            newfile.Size = (((((buf[31] << 8) + buf[30]) << 8) + buf[29]) << 8) + buf[28];
            newfile.FirstClus = (((((buf[21] << 8) + buf[20]) << 8) + buf[27]) << 8) + buf[26];
            for(int i = 0; i < 8 && newfile.ShortName[i] != ' '; ++i)
                newfile.LongName += newfile.ShortName[i];
            newfile.LongName += L'.';
            for(int i = 8; i < 11 && newfile.ShortName[i] != ' '; ++i)
                newfile.LongName += newfile.ShortName[i];
            if(newfile.LongName[newfile.LongName.length() - 1] == '.')
                newfile.LongName.erase(newfile.LongName.end() - 1);
            if((buf[11] & 0x18) == 0x00) {
                //File found
                newfile.Dir = false;
            }
            else if((buf[11] & 0x18) == 0x08) {
                //Root dir found
                continue;
            }
            else if((buf[11] & 0x18) == 0x10) {
                //Dir found
                newfile.Dir = true;
            }
            else {
                //Invalid dir
                continue;
            }
        }
        for(size_t i = 0; i < newfile.LongName.length(); ++i) {
            if(newfile.LongName[i] >= L'A' && newfile.LongName[i] <= L'Z')
                newfile.id += newfile.LongName[i] - L'A' + L'a';
            else
                newfile.id += newfile.LongName[i];
        }
        F.push_back(newfile);
    }
    sort(F.begin(), F.end(), cmp_id);
    return F;
}

int main(int argc, char* argv[]) {
	if(argc == 1) {
        cout << "Error: File system not specified" << endl;
        return 0;
    }
    else {
        f = fopen(argv[1], "rb");
        if(!f) {
            cout << "Error: can not open the file." << endl;
            return 0;
        }
    }

	wcout.imbue(locale("rus_rus.866"));
	wcin.imbue(locale("rus_rus.866"));
	
    unsigned char buf[32769];
    fread(buf, 64, 1, f);
    BPB_BytsPerSec = (buf[12] << 8) + buf[11];
    BPB_SecPerClus = buf[13];
    BPB_RsvdSecCnt = (buf[15] << 8) + buf[14];
    BPB_NumFATs = buf[16];
    BPB_RootEntCnt = (buf[18] << 8) + buf[17];
    BPB_TotSec16 = (buf[20] << 8) + buf[19];
    BPB_FATSz16 = (buf[23] << 8) + buf[22];
    BPB_TotSec32 = (((((buf[35] << 8) + buf[34]) << 8) + buf[33]) << 8) + buf[32];
    if(BPB_FATSz16 == 0) {
        BPB_FATSz32 = (((((buf[39] << 8) + buf[38]) << 8) + buf[37]) << 8) + buf[36];
    }

    if(BPB_FATSz16 != 0)
        FATSz = BPB_FATSz16;
    else
        FATSz = BPB_FATSz32;

    if(BPB_TotSec16 != 0)
        TotSec = BPB_TotSec16;
    else
        TotSec = BPB_TotSec32;

    RootDirSectors = ((BPB_RootEntCnt * 32) + (BPB_BytsPerSec - 1)) / BPB_BytsPerSec;
    FirstDataSector = BPB_RsvdSecCnt + (BPB_NumFATs * FATSz) + RootDirSectors;
    DataSec = TotSec - (BPB_RsvdSecCnt + (BPB_NumFATs * FATSz) + RootDirSectors);
    CountofClusters = DataSec / BPB_SecPerClus;

    if(CountofClusters < 4085) {
        cout << argv[1] << " is not a valid FAT filesystem" << endl;
        return 0;
    }
    else if(CountofClusters < 65525) {
        FATType = FAT16;
    } else {
        FATType = FAT32;
    }

    if(FATType == FAT32) {
        BPB_RootClus = (((((buf[47] << 8) + buf[46]) << 8) + buf[45]) << 8) + buf[44];
        RootDirSecNum = FirstSectorofCluster(BPB_RootClus);
    }
    else {
        RootDirSecNum = BPB_RsvdSecCnt + (BPB_NumFATs * BPB_FATSz16);
    }

    GetFatTable();

    vector <file> F;

    string action;
    CurDirSecNum = RootDirSecNum;
    wstring path = L"/> ";
    wcout << path;
    cin >> action;
    while(action != "exit" && action != "quit") {
        F = ScanFolder(CurDirSecNum, ClusterofSector(CurDirSecNum));
        if(action == "ls") {
            for(size_t i = 0; i < F.size(); ++i) {
                if(F[i].Dir)
                    printf("[DIR]   ");
                else
                    printf("        ");
                wcout << F[i].LongName << endl;
            }
        }
        else if(action == "go") {
            // go to direct cluster
            // DO NOT USE!
            int num;
            cin >> num;
            CurDirSecNum = FirstSectorofCluster(num);
        }

        else if(action == "cd") {
            wstring name, addition;
            wcin >> name;
            getline(wcin, addition);
            name += addition;
            for(size_t i = 0; i < name.length(); ++i) {
                if(name[i] >= L'A' && name[i] <= L'Z')
                    name[i] = name[i] - L'A' + L'a';
            }
            bool FileFound = false;
            for(size_t i = 0; i < F.size(); ++i)
                if(F[i].id == name) {
					FileFound = true;
                    if(!F[i].Dir)
                        cout << "This is not a folder" << endl;
                    else {
                        path.erase(path.end()-2, path.end());
                        if(name == L"..") {
                            for(size_t i = path.length() - 1; path[i] != L'/'; --i)
                                path.erase(path.end()-1);
                            if(path.length() > 1)
                                path.erase(path.end()-1);
                        }
                        else if(name != L".") {
                            if(path.length() > 1)
                                path += L"/";
                            path += F[i].LongName;
                        }
                        path += L"> ";
                        CurDirSecNum = FirstSectorofCluster(F[i].FirstClus);
                    }
					break;
                }
            if(!FileFound)
                cout << "There is no such folder" << endl;
        }
        else if(action == "help") {
			cout << "========================================================" << endl << endl;
            cout << "FAT Reader 0.05" << endl << "by Mikhail Melnik" << endl;
            cout << endl << "========================================================" << endl << endl;
            cout << "ls                         -   get the list of files in current dir" << endl;
            cout << "cd [destination]           -   change directory" << endl;
            cout << "help                       -   view this guide" << endl;
            cout << "copy [file]                -   copy file from current dir to external place" << endl;
            cout << "cat [file]                 -   show the file" << endl;
            cout << "exit                       -   exit program" << endl;
            cout << "quit                       -   exit program" << endl << endl;
            cout << "========================================================" << endl;
        }
        else if(action == "cat") {
            wstring name, addition;
            wcin >> name;
            getline(wcin, addition);
            name += addition;
            for(size_t i = 0; i < name.length(); ++i) {
                if(name[i] >= L'A' && name[i] <= L'Z')
                    name[i] = name[i] - L'A' + L'a';
            }

            bool FileFound = false;
            for(size_t i = 0; i < F.size(); ++i)
                if(F[i].id == name) {
                    FileFound = true;
                    if(F[i].Dir)
                        wcout << F[i].LongName << L" is a folder. Choose the file" << endl;
                    else {
                        unsigned int size = F[i].Size, clus = F[i].FirstClus;
						cout << endl;
                        while(size >= BPB_BytsPerSec*BPB_SecPerClus) {
	                        fseek64(f, FirstSectorofCluster(clus)*BPB_BytsPerSec, SEEK_SET);
	                        fread(buf, BPB_BytsPerSec*BPB_SecPerClus, 1, f);
	                        size -= BPB_BytsPerSec*BPB_SecPerClus;
                            cout.write((char *)buf, BPB_BytsPerSec*BPB_SecPerClus);
                            clus = FindNextClus(clus);
                            if(clus == 0xFFFFFFF7) {
				                cout << endl << "Bad cluster found!" << endl;
				                size = 0;
				                break;
				            }
				            else if(clus == 0xFFFFFFFF) {
				            	break;
				            }
                        }
                        if(size > 0) {
	                        fseek64(f, FirstSectorofCluster(clus)*BPB_BytsPerSec, SEEK_SET);
	                        fread(buf, size, 1, f);
                            cout.write((char *)buf, size);
                        }
                        cout << endl << endl;
                        break;
                    }
                }
            if(!FileFound)
                cout << "File not found" << endl;
        }
        else if(action == "copy") {
            wstring name, addition;
            wcin >> name;
            getline(wcin, addition);
            name += addition;
            for(size_t i = 0; i < name.length(); ++i) {
                if(name[i] >= L'A' && name[i] <= L'Z')
                    name[i] = name[i] - L'A' + L'a';
            }
            bool FileFound = false;
            for(size_t i = 0; i < F.size(); ++i)
                if(F[i].id == name) {
                    FileFound = true;
                    if(F[i].Dir)
                        wcout << F[i].LongName << L" is a folder. Choose the file" << endl;
                    else {
                        cout << "Specify destination directory" << endl;
                        wstring dest;
                        getline(wcin, dest);
                        FILE *d;
                        if(dest[dest.length()-1] != L'/')
                            dest += L'/';
                        wchar_t DEST[512];
                        for(size_t j = 0; j < dest.length(); ++j)
                            DEST[j] = dest[j];
                        for(size_t j = 0; j < F[i].LongName.length(); ++j)
                            DEST[dest.length() + j] = F[i].LongName[j];
                        DEST[dest.length() + F[i].LongName.length()] = 0;
                        d = _wfopen(DEST, L"wb");
                        unsigned int size = F[i].Size, clus = F[i].FirstClus;
                        while(size >= BPB_BytsPerSec*BPB_SecPerClus) {
	                        fseek64(f, FirstSectorofCluster(clus)*BPB_BytsPerSec, SEEK_SET);
	                        fread(buf, BPB_BytsPerSec*BPB_SecPerClus, 1, f);
	                        size -= BPB_BytsPerSec*BPB_SecPerClus;
                            fwrite(buf, BPB_BytsPerSec*BPB_SecPerClus, 1, d);
                            clus = FindNextClus(clus);
                            if(clus == 0xFFFFFFF7) {
				                cout << endl << "Bad cluster found!" << endl;
				                size = 0;
				                break;
				            }
				            else if(clus == 0xFFFFFFFF) {
				            	break;
				            }
                        }
                        if(size > 0) {
	                        fseek64(f, FirstSectorofCluster(clus)*BPB_BytsPerSec, SEEK_SET);
	                        fread(buf, size, 1, f);
                            fwrite(buf, size, 1, d);
                        }
                        fclose(d);
                        break;
                    }
                }
            if(!FileFound)
                cout << "File not found" << endl;
        }
        else if(action == "dragon") {
            cout << "                                        ,   ," << endl;
            cout << "                                        $,  $,     ," << endl;
            cout << "                                        \"ss.$ss. .s'" << endl;
            cout << "                                ,     .ss$$$$$$$$$$s," << endl;
            cout << "                                $. s$$$$$$$$$$$$$$`$$Ss" << endl;
            cout << "                                \"$$$$$$$$$$$$$$$$$$o$$$       ," << endl;
            cout << "                               s$$$$$$$$$$$$$$$$$$$$$$$$s,  ,s" << endl;
            cout << "                              s$$$$$$$$$\"$$$$$$\"\"\"\"$$$$$$\"$$$$$," << endl;
            cout << "                              s$$$$$$$$$$s\"\"$$$$ssssss\"$$$$$$$$\"" << endl;
            cout << "                             s$$$$$$$$$$'         `\"\"\"ss\"$\"$s\"\"" << endl;
            cout << "                             s$$$$$$$$$$,              `\"\"\"\"\"$  .s$$s" << endl;
            cout << "                             s$$$$$$$$$$$$s,...               `s$$'  `" << endl;
            cout << "                         `ssss$$$$$$$$$$$$$$$$$$$$####s.     .$$\"$.   , s-" << endl;
            cout << "                           `\"\"\"\"$$$$$$$$$$$$$$$$$$$$#####$$$$$$\"     $.$'" << endl;
            cout << "                                 \"$$$$$$$$$$$$$$$$$$$$$####s\"\"     .$$$|" << endl;
            cout << "                                  \"$$$$$$$$$$$$$$$$$$$$$$$$##s    .$$\" $" << endl;
            cout << "                                   $$\"\"$$$$$$$$$$$$$$$$$$$$$$$$$$$$$\"   `" << endl;
            cout << "                                  $$\"  \"$\"$$$$$$$$$$$$$$$$$$$$S\"\"\"\"'" << endl;
            cout << "                             ,   ,\"     '  $$$$$$$$$$$$$$$$####s" << endl;
            cout << "                             $.          .s$$$$$$$$$$$$$$$$$####\"" << endl;
            cout << "                 ,           \"$s.   ..ssS$$$$$$$$$$$$$$$$$$$####\"" << endl;
            cout << "                 $           .$$$S$$$$$$$$$$$$$$$$$$$$$$$$#####\"" << endl;
            cout << "                 Ss     ..sS$$$$$$$$$$$$$$$$$$$$$$$$$$$######\"\"" << endl;
            cout << "                  \"$$sS$$$$$$$$$$$$$$$$$$$$$$$$$$$########\"" << endl;
            cout << "           ,      s$$$$$$$$$$$$$$$$$$$$$$$$#########\"\"'" << endl;
            cout << "           $    s$$$$$$$$$$$$$$$$$$$$$#######\"\"'      s'         " << endl;
            cout << "           $$..$$$$$$$$$$$$$$$$$$######\"'       ....,$$....    ,$" << endl;
            cout << "            \"$$$$$$$$$$$$$$$######\"' ,     .sS$$$$$$$$$$$$$$$$s$$" << endl;
            cout << "              $$$$$$$$$$$$#####\"     $, .s$$$$$$$$$$$$$$$$$$$$$$$$s." << endl;
            cout << "   )          $$$$$$$$$$$#####'      `$$$$$$$$$###########$$$$$$$$$$$." << endl;
            cout << "  ((          $$$$$$$$$$$#####       $$$$$$$$###\"       \"####$$$$$$$$$$" << endl;
            cout << "  ) \\         $$$$$$$$$$$$####.     $$$$$$###\"             \"###$$$$$$$$$   s'" << endl;
            cout << " (   )        $$$$$$$$$$$$$####.   $$$$$###\"                ####$$$$$$$$s$$'" << endl;
            cout << " )  ( (       $$\"$$$$$$$$$$$#####.$$$$$###'                .###$$$$$$$$$$\"" << endl;
            cout << " (  )  )   _,$\"   $$$$$$$$$$$$######.$$##'                .###$$$$$$$$$$" << endl;
            cout << " ) (  ( \\.         \"$$$$$$$$$$$$$#######,,,.          ..####$$$$$$$$$$$\"" << endl;
            cout << "(   )$ )  )        ,$$$$$$$$$$$$$$$$$$####################$$$$$$$$$$$\"" << endl;
            cout << "(   ($$  ( \\     _sS\"  `\"$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$S$$," << endl;
            cout << " )  )$$$s ) )  .      .   `$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$\"'  `$$" << endl;
            cout << "  (   $$$Ss/  .$,    .$,,s$$$$$$##S$$$$$$$$$$$$$$$$$$$$$$$$S\"\"        '" << endl;
            cout << "    \\)_$$$$$$$$$$$$$$$$$$$$$$$##\"  $$        `$$.        `$$." << endl;
            cout << "        `\"S$$$$$$$$$$$$$$$$$#\"      $          `$          `$" << endl;
            cout << "            `\"\"\"\"\"\"\"\"\"\"\"\"\"'         '           '           '" << endl;
		}
        else {
            cout << action << ": command not found" << endl;
        }
        wcout << path;
        cin >> action;
    }

    fclose(f);

    return 0;
}