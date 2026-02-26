// 🚀 优化版 main.cpp - 性能改进版本
// 改进内容: 删除不必要延迟，改进用户交互

#include "Tollbox.h"
using namespace std;

int main()
{
    std::cout << "\x1b[36m" << std::flush;

    // 创建密钥并立即交给 AES 对象
    std::vector<uint8_t> key = generateRandomKey();
    AES256 aes(key);

    // 生成受保护的认证密文
    {
        std::string sec = get_secure_string();
        std::vector<uint8_t> sec_bytes(sec.begin(), sec.end());
        std::vector<uint8_t> hash_bytes = SecureHash::hashBytes(sec_bytes, 32);
        std::vector<uint8_t> correct_aead = aes.encryptAEAD(hash_bytes);

        secure_clean(sec);
        std::fill(sec_bytes.begin(), sec_bytes.end(), 0);
        std::fill(hash_bytes.begin(), hash_bytes.end(), 0);

        stored_correct_aead = std::move(correct_aead);

        std::fill(key.begin(), key.end(), 0);
        key.clear();
        key.shrink_to_fit();
    }

    string user_input;
    size_t cnt_er = { 0 };
    size_t cnt_er_er = { 0 };
    const auto max_er{ 2 };

    std::string filePath = "E:\\C++\\test.txt";
    std::string filePath_game = "E:\\C++\\game.txt";

    std::ofstream outFile(filePath, ios::app);
    std::fstream max_f(filePath_game, std::ios::in | std::ios::out | ios::app);

    if (!max_f.is_open()) {
        std::cerr << "无法打开文件！" << std::endl;
        return -1;
    }

    if (!outFile.is_open()) {
        std::cerr << "无法创建文件！" << std::endl;
        return -1;
    }

    // ✅ 优化1: 直接显示，无不必要延迟
    cout << "已启动加密算法" << endl;
    cout << endl;

    cerr << "输入密码以访问加密程序\n>>";

    while (true) {
        cin.clear();
        getline(cin, user_input);
        cout << "当前输入密文强度:" << calculateKeyEntropy(user_input) << endl;

        std::vector<uint8_t> user_bytes(user_input.begin(), user_input.end());
        std::vector<uint8_t> user_hash = SecureHash::hashBytes(user_bytes, 32);

        bool auth_ok = false;
        try {
            std::vector<uint8_t> recovered = aes.decryptAEAD(stored_correct_aead);
            if (recovered.size() == user_hash.size()) {
                unsigned char diff = 0;
                for (size_t i = 0; i < recovered.size(); ++i) diff |= recovered[i] ^ user_hash[i];
                auth_ok = (diff == 0);
            }

            std::fill(recovered.begin(), recovered.end(), 0);
        }
        catch (...) {
            auth_ok = false;
        }

        std::fill(user_bytes.begin(), user_bytes.end(), 0);
        std::fill(user_hash.begin(), user_hash.end(), 0);

        if (auth_ok) {
            outFile << "Key is right!" << endl;
            outFile << "Time:" << getCurrentTime() << endl;
            outFile.close();

            secure_clean(user_input);
            break;
        }
        else {
            cerr << "\x1b[31m密码错误，请重新输入\x1b[36m\n>>";
            cin.clear();
            ++cnt_er;
            cout.clear();

            if (cnt_er > max_er) {
                cin.clear();
                cout.clear();
                ++cnt_er_er;

                // ✅ 优化2: 改进的锁定机制 - 使用渐进式倒计时
                size_t cnt_time = 13;
                clog << "\x1b[31m密码错误过多，触发锁定机制，等待解除\x1b[36m" << endl;

                for (size_t i = cnt_time; i > 0; --i) {
                    clog << "剩余: " << i << "s\r" << flush;
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                }

                cnt_er = 0;
                clog << "\x1b[32m锁定解除，请重新输入\x1b[36m\n>>" << flush;
                cout << endl;
            }
            else {
                if (cnt_er_er > max_er) {
                    cerr << "Error count too high, process aborted." << endl;
                    return -1;
                }
            }
        }
        cin.clear();
    };

    clog << "Yes" << endl;
    // ✅ 优化3: 删除无用的sleep(467)，直接进入加载

    // 清屏
    clog << "\x1b[2J\x1b[1;1H" << flush;

    {
        cout << "正在加载……" << endl;
        const int len = 100;
        for (int i = 0; i < len; i++)
        {
            simple_progress(i, len, 30);
            sleep(40, 1);
        }
        cout << endl;
        cout << "加载成功" << endl;
    }

    while (true)
    {
        size_t cnt = game();
        clog << "你总共猜测了 " << cnt << " 次。" << endl;

        size_t tump = 0;
        max_f.seekp(0, std::ios::beg);

        max_f >> tump;
        max_f.close();

        if (tump > cnt)
        {
            max_f.open(filePath_game, std::ios::in | std::ios::out | ios::trunc);
            max_f.seekp(0, std::ios::beg);
            max_f << cnt << flush;
            cout << "恭喜你，打破了最高分记录！" << endl;
        }

        max_f.seekp(0, std::ios::beg);

        max_f >> tump;
        cout << "最高分:" << tump << endl;

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

    std::cout << "\x1b[0m" << std::endl;
    return 0;
}
