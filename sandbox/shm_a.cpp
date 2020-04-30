#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h>


class vnc_client
{
public:
    int sockfd;
    std::string host;
    int port;
    std::string password;

    vnc_client() {
        this->sockfd = 0;
        this->host = "";
        this->port = 0;
        this->password = "";
    };
};

using namespace std;

int main(){
    FILE *fp;
    const string file_path = "./key.dat";
    fp = fopen(file_path.c_str(), "w");
    fclose(fp);

    // IPC key
    const int id = 50;
    const key_t key = ftok(file_path.c_str(), id);
    if(key == -1){
        cerr << "Failed to acquire key" << endl;
        return EXIT_FAILURE;
    }

    // shm id
    const int size = 0x6400;
    const int seg_id = shmget(key, size,
                              IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);
    if(seg_id == -1){
        cerr << "Failed to acquire segment" << endl;
        return EXIT_FAILURE;
    }

    // attach to process
    vnc_client *v = new vnc_client();
    char* const shared_memory = reinterpret_cast<char*>(shmat(seg_id, 0, 0));

    // write to shared memory
    string s;

    int flag = 0;
    cout << "if you want to close, please type 'q'" << endl;
    while(flag == 0){
        cout << "word: ";
        cin >> s;
        if(s == "q") flag = 1;
        else {
            sprintf(shared_memory, s.c_str());
        }
    }

    // detach from process
    shmdt(shared_memory);

    // free shared memory
    shmctl(seg_id, IPC_RMID, NULL);

    return 0;
}
