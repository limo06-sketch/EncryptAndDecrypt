#include "Tollbox.h"
using namespace std;

int main()
{
    std::cout << "\x1b[36m" << std::flush;
    // 创建密钥并立即交给 AES 对象（最小化密钥在栈上的生命周期）
    std::vector<uint8_t> key = generateRandomKey();
    AES256 aes(key);
    
    // 生成受保护的认证密文（使用 AEAD: Encrypt-then-MAC）
    {
        // 计算原始哈希字节（32 bytes）并加密
        std::string sec = get_secure_string();
        std::vector<uint8_t> sec_bytes(sec.begin(), sec.end());
        std::vector<uint8_t> hash_bytes = SecureHash::hashBytes(sec_bytes, 32);
        // encryptAEAD will produce ciphertext (IV||ct) || mac
        std::vector<uint8_t> correct_aead = aes.encryptAEAD(hash_bytes);

        // overwrite plaintext-sensitive buffers
        secure_clean(sec);
        std::fill(sec_bytes.begin(), sec_bytes.end(), 0);
        std::fill(hash_bytes.begin(), hash_bytes.end(), 0);

        // keep correct_aead in file-scope variable for verification
        stored_correct_aead = std::move(correct_aead);

        // clear original key material in local variable
        std::fill(key.begin(), key.end(), 0);
        key.clear(); key.shrink_to_fit();
    }

	string user_input;
    size_t cnt_er = { 0 };
    size_t cnt_er_er = { 0 };
    const auto max_er { 2 };
    // 指定文件路径，注意：使用双反斜杠
    std::string filePath = "E:\\C++\\test.txt";

    // 创建并打开文件
    std::ofstream outFile(filePath, ios::app);

    // 检查文件是否成功打开
    if (!outFile.is_open()) {
        std::cerr << "无法创建文件！" << std::endl;
        return -1;
    }

    cout << "已启动加密算法" << endl;
    cout << endl;
    sleep(400, 4);

    cerr << "输入密码以访问加密程序\n>>";
    while (true) {
        cin.clear();
        getline(cin, user_input);
        cout << "当前输入密文强度:" << calculateKeyEntropy(user_input) << endl;

        // compute hash bytes of user input
        std::vector<uint8_t> user_bytes(user_input.begin(), user_input.end());
        std::vector<uint8_t> user_hash = SecureHash::hashBytes(user_bytes, 32);

        // retrieve stored AEAD (from file-scope variable)
        // verify by decrypting AEAD
        bool auth_ok = false;
        try {
            std::vector<uint8_t> recovered = aes.decryptAEAD(stored_correct_aead);
            // constant-time compare recovered vs user_hash
            if (recovered.size() == user_hash.size()) {
                unsigned char diff = 0;
                for (size_t i = 0; i < recovered.size(); ++i) diff |= recovered[i] ^ user_hash[i];
                auth_ok = (diff == 0);
            }

            // clear recovered
            std::fill(recovered.begin(), recovered.end(), 0);
        }
        catch (...) {
            auth_ok = false;
        }

        // clear temp sensitive buffers
        std::fill(user_bytes.begin(), user_bytes.end(), 0);
        std::fill(user_hash.begin(), user_hash.end(), 0);

        if (auth_ok){
            outFile << "Key is right!" << endl;
            outFile << "Time:" << getCurrentTime() << endl;
            outFile.close();

            secure_clean(user_input);
            break;
        }else{
            cerr << "\x1b[31m密码错误，请重新输入\x1b[36m\n>>";
            cin.clear();
            ++cnt_er;
            cout.clear();
            if (cnt_er>max_er)
            {
                cin.clear();
                cout.clear();
                ++cnt_er_er;
                size_t cnt_time = { 13 };
                clog << "密码错误过多，触发锁定机制，等待" << cnt_time<<"秒！" << endl;
                while (cnt_time>0)
                {
                    clog << "剩余:" << cnt_time <<"s!\r" << flush;
                    sleep(1000,2);
                    --cnt_time;
                }
                cnt_er = 0;
                cout << endl;
            }
            else {
                if (cnt_er_er > max_er)
                {
                    cerr << "Error count too high, process aborted." << endl;
                    return -1;
                }
            }
        }
        cin.clear();
    };
    clog << "Yes" << endl;
	sleep(467, 4);
	clog << "\x1b[2J\x1b[1;1H" << flush; // 清屏

    {
        cout << "正在加载……" << endl;
        const int len = 100;
        for (int i = 0; i < len; i++)
        {
            simple_progress(i, len, 30);
            sleep(100, 3);
        }
        cout << endl;
        cout << "加载成功" << endl;
    }
    while (true)
    {
        size_t cnt = game();
        clog << "你总共猜测了 " << cnt << " 次。" << endl;
        clog << "是否再来一局？(y/n): ";
        char choice;
        cin >> choice;
        if (choice == 'y' || choice == 'Y') {
            continue;
        }
        else {
            break;
		}
    }
    //cout << "测试成功" << endl;
    std::cout << "\x1b[0m" << std::endl;
    return 0;
}