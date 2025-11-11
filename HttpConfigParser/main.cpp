#include "HttpConfigParser.hpp"
#include <iostream>
#include <vector>

/*
** Day 1 のゴール（トークナイザーの確認）を実行するメイン関数
*/
// int main(int argc, char **argv) {
//     // 実行時に .conf ファイルが指定されているかチェック
//     if (argc != 2) {
//         std::cerr << "Usage: " << argv[0] << " <config_file>" << std::endl;
//         return 1;
//     }

//     std::string filename = argv[1];

//     try {
//         // タスク4で実装するコンストラクタを呼び出す
//         HttpConfigParser parser(filename);

//         // トークンリストを取得して表示
//         // ※注: HttpConfigParser.hpp に getTokens() を追加する必要があります
//         const std::vector<std::string>& tokens = parser.getTokens();

//         std::cout << "--- Tokenizer Result ---" << std::endl;
//         std::cout << "[";
//         for (size_t i = 0; i < tokens.size(); ++i) {
//             std::cout << "\"" << tokens[i] << "\"";
//             if (i < tokens.size() - 1) {
//                 std::cout << ", ";
//             }
//         }
//         std::cout << "]" << std::endl;
//         std::cout << "------------------------" << std::endl;

//     } catch (const std::exception& e) {
//         std::cerr << "Error: " << e.what() << std::endl;
//         return 1;
//     }
//     return 0;
// }

// main.cpp
#include "HttpConfigParser.hpp" // HttpConfig.h はこれがインクルードしている
#include <iostream>
#include <vector>
#include <string>

// --- プロトタイプ宣言 (mainより前に置く) ---
void printConfig(const HttpConfig& config);
void printCommonConfig(const CommonConfig& common, const std::string& indent);
void printLocationConfig(const LocationConfig& location, const std::string& indent);
void printServerConfig(const ServerConfig& server, const std::string& indent);

/**
 * @brief メイン関数
 */
int main(int argc, char **argv) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <config_file>" << std::endl;
        return 1;
    }

    try {
        // [新] HttpConfigParser クラスから、直接 ::parse() を呼び出す
        HttpConfig config = HttpConfigParser::parse(argv[1]); 
        
        // --- 実際にパースした内容を表示する ---
        std::cout << "--- Config Parse Result ---" << std::endl;
        printConfig(config);
        std::cout << "---------------------------" << std::endl;
        // --- ここまで ---

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl; 
        return 1;
    }
    return 0;
}

// --- 以下、表示用ヘルパー関数 ---

/**
 * @brief 共通設定(CommonConfig) の内容を表示
 */
void printCommonConfig(const CommonConfig& common, const std::string& indent) {
    std::cout << indent << "Root: " << common.getRoot() << std::endl;
    std::cout << indent << "Autoindex: " << (common.getAutoindex() ? "on" : "off") << std::endl;
    
    const std::vector<std::string>& indexes = common.getIndexFiles();
    std::cout << indent << "Index: ";
    if (indexes.empty()) {
        std::cout << "(empty)";
    } else {
        for (size_t i = 0; i < indexes.size(); ++i) {
            std::cout << indexes[i] << (i == indexes.size() - 1 ? "" : " ");
        }
    }
    std::cout << std::endl;
}

/**
 * @brief LocationConfig の内容を表示
 */
void printLocationConfig(const LocationConfig& location, const std::string& indent) {
    std::cout << indent << "Location: " << location.getPath() << " {" << std::endl;
    printCommonConfig(location, indent + "  "); 
    std::cout << indent << "}" << std::endl;
}

/**
 * @brief ServerConfig の内容を表示
 */
void printServerConfig(const ServerConfig& server, const std::string& indent) {
    const ListenDirective& ld = server.getListen();
    std::cout << indent << "Listen: " << ld.address << ":" << ld.port 
              << (ld.is_default_server ? " (default)" : "") << std::endl;
    
    printCommonConfig(server, indent);

    const std::vector<LocationConfig>& locations = server.getLocations();
    for (size_t i = 0; i < locations.size(); ++i) {
        printLocationConfig(locations[i], indent + "  ");
    }
}

/**
 * @brief HttpConfig の内容 (全サーバー) を表示
 */
void printConfig(const HttpConfig& config) {
    const std::vector<ServerConfig>& servers = config.getServers();
    
    if (servers.empty()) {
        std::cout << "(No server blocks found)" << std::endl;
        return;
    }
    
    for (size_t i = 0; i < servers.size(); ++i) {
        std::cout << "Server " << (i + 1) << " {" << std::endl;
        printServerConfig(servers[i], "  ");
        std::cout << "}" << std::endl;
        if (i < servers.size() - 1) {
            std::cout << std::endl;
        }
    }
}