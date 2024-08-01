


// int main2(int argc, char *argv[]) {
// 	InitSyscall();
// 	GlobalInit();
// 	long args[6];
// 	const char* pathname = "/curvefs/test_mount/testfile";
// 	args[0] = (long)(pathname);
// 	args[1] = O_CREAT | O_WRONLY | O_TRUNC;
// 	args[2] = S_IRUSR | S_IWUSR;
// 	long result = 0;
// 	PosixOpOpen(args, &result);
// 	PosixOpAccess(args, &result);
// 	return 0;
// }

#include "registry/client_server_registry.h"
// ! 暂时注释，使用时不能注释
#include "posix/posix_helper.h"
using intercept::middleware::ReqResMiddlewareWrapper;
int main() {
    constexpr char APP_NAME[] = "iox-intercept-client";
    iox::runtime::PoshRuntime::initRuntime(APP_NAME);

    intercept::internal::ServiceMetaInfo info;
    info.service = SERVICE_FLAG;
    info.instance = INTERCEPT_INSTANCE_FLAG;
    intercept::registry::ClientServerRegistry registry(ICEORYX, info);
    auto dummyserver = registry.CreateDummyServer();
    sleep(2);

    info = dummyserver->GetServiceMetaInfo();
    info.service = SERVICE_FLAG;
    info.instance = INTERCEPT_INSTANCE_FLAG;
    
    std::shared_ptr<ReqResMiddlewareWrapper> wrapper = registry.CreateClient(info);

    intercept::internal::OpenOpReqRes req("/testdir/hellofile1", O_CREAT|O_RDWR, S_IRUSR | S_IWUSR);
    wrapper->OnRequest(req);
    const auto& openRes = static_cast<intercept::internal::OpenResponseData&>  (req.GetResponse());


    char* writebuf = (char*)malloc(sizeof(char) * 1024);
    char str[] = "hello world";
    memcpy(writebuf, str, sizeof(str));
    intercept::internal::WriteOpReqRes writeReq(openRes.fd, writebuf, strlen(writebuf));
    wrapper->OnRequest(writeReq);

    // open and read
    intercept::internal::OpenOpReqRes req2("/testdir/hellofile1", O_RDWR, S_IRUSR | S_IWUSR);
    wrapper->OnRequest(req2);
    const auto& openRes2 = static_cast<intercept::internal::OpenResponseData&>  (req2.GetResponse());
    char* buf2 = (char*)malloc(sizeof(char) * 1024);
     
    intercept::internal::ReadOpReqRes readReq2(openRes2.fd, buf2, 8);
    wrapper->OnRequest(readReq2);
    free((void*)buf2);

    dummyserver->StopServer();
    std::cout << "stop dummyserver in main" << std::endl;
    //sleep(5);
    return 0;
}

int mainposix() {
    char filename[256];

    // 循环执行流程
    while (true) {
        std::cout << "Enter filename (or 'exit' to quit): ";
        std::cin >> filename;

        if (strcmp(filename, "exit") == 0) {
            std::cout << "Exiting program..." << std::endl;
            break;
        }

        std::cout << "Enter 'write' to write to file, 'read' to read from file: ";
        std::string operation;
        std::cin >> operation;

        if (operation == "write") {
            // 打开文件进行写入
            int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
            if (fd == -1) {
                std::cerr << "Error: Failed to open file for writing." << std::endl;
                continue;
            }

            std::string content;
            std::cout << "Enter content to write to file (end with 'EOF'): " << std::endl;
            std::cin.ignore(); // 忽略前一个输入的换行符
            while (true) {
                std::string line;
                std::getline(std::cin, line);
                if (line == "EOF") {
                    break;
                }
                content += line + "\n";
            }

            ssize_t bytes_written = write(fd, content.c_str(), content.size());
            std::cout << "the write byte: " << bytes_written << std::endl;
            close(fd);
        } else if (operation == "read") {
            // 打开文件进行读取
            int fd = open(filename, O_RDONLY);
            if (fd == -1) {
                std::cerr << "Error: Failed to open file for reading." << std::endl;
                continue;
            }

            char buffer[4096];
            ssize_t bytesRead;
            std::cout << "Content read from file:" << std::endl;
            while ((bytesRead = read(fd, buffer, sizeof(buffer))) > 0) {
                std::cout.write(buffer, bytesRead);
            }
            std::cout << std::endl;

            // 获取文件的状态信息
            struct stat fileStat;
            if (fstat(fd, &fileStat) == 0) {
                std::cout << "File size: " << fileStat.st_size << " bytes" << std::endl;
                std::cout << "File permissions: " << (fileStat.st_mode & (S_IRWXU | S_IRWXG | S_IRWXO)) << std::endl;
            } else {
                std::cerr << "Error: Failed to get file status." << std::endl;
            }

            close(fd);
        } else {
            std::cerr << "Error: Invalid operation. Please enter 'write' or 'read'." << std::endl;
        }
    }

    return 0;
}
