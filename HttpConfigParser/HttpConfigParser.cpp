#include "HttpConfigParser.hpp"
#include <cctype> // std::isspace
#include <sstream>//parseListen用

// --- コンストラクタとデストラクタを削除 (または private 実装) ---
HttpConfigParser::HttpConfigParser() {}
HttpConfigParser::~HttpConfigParser() {}


//終端に来たかどうか
bool HttpConfigParser::isEof(const std::vector<std::string>& tokens, size_t index) {
	return index >= tokens.size();
}

//次のトークンを取得し、インデックスを1つ進める
std::string HttpConfigParser::getNextToken(const std::vector<std::string>& tokens, size_t& index) {
	if (isEof(tokens, index)) {
		throw std::runtime_error("Error: Unexpected end of EOF");
	}
	return tokens[index++];
}

//failを読み込み、トークン化する
std::vector<std::string> HttpConfigParser::tokenize(const std::string& filename){
	std::ifstream ifs(filename.c_str());
	if (!ifs.is_open()) {
		throw std::runtime_error("Error: Could not open file " + filename);
	}

	std::vector<std::string> tokens;
	std::string line;
	std::string special_chars = "{};";

	while (std::getline(ifs, line)) {
		for (size_t i = 0; i < line.size(); ++i) {
			// 空欄スキップ
			if (std::isspace(line[i])) {continue;}
			// コメント行スキップ
			if (line[i] == '#') {break;}
			// トークン抽出
			if (special_chars.find(line[i]) != std::string::npos) {
                tokens.push_back(line.substr(i, 1));
                continue;
            }
			size_t start = i;
			while(i < line.size() && 
                  !std::isspace(line[i]) &&
                  line[i] != '#' &&
                  special_chars.find(line[i]) == std::string::npos)
            {
                ++i;
            }
            tokens.push_back(line.substr(start, i - start));
            --i;
        }
	}
	return tokens;	
}


//-----------------------------------------------------------
//------------------parser本体-------------------------------
//-----------------------------------------------------------
///@brief パースを実行する唯一の public インターフェース
///@param filename 設定ファイルパス
///@return 完成した HttpConfig オブジェクト

HttpConfig HttpConfigParser::parse(const std::string& filename){
	// トークン化 全トークンを取得
	std::vector<std::string> tokens = HttpConfigParser::tokenize(filename);

	//parse処理に必要な道具をローカルに準備
	HttpConfig config;//完成させるHttpConfigオブジェクト
	size_t index = 0;//現在のトークン位置

	//メインの解析ループ
	// indexが終端(tokens.size())に達するまでループ
	while(!HttpConfigParser::isEof(tokens, index)) {
		//tokensを一つ取り出して、indexを進める
		std::string token = HttpConfigParser::getNextToken(tokens, index);

		//serverブロック以外はエラー
		if (token == "server") {
			HttpConfigParser::parserServer(config, tokens, index);
		} else {
			throw std::runtime_error("Error: Unknown directive outside server block:" + token);
		}
	}
	
	return config;

}

//-----------------------------------------------------------
//------------------サーバーブロックパーサー-------------------
//-----------------------------------------------------------

///@brief server ブロックをパースする
///@param config HttpConfig オブジェクト (参照渡し)
///@param tokens トークンリスト
///@param index 現在の位置 (参照渡しで、この値が内部で進められる)

void HttpConfigParser::parserServer(HttpConfig& config, const std::vector<std::string>& tokens, size_t& index) {
	//server の後は '{' が来るはず
	if(HttpConfigParser::getNextToken(tokens, index) != "{") {
		throw std::runtime_error("Error: Expected '{' after server directive");
	}

	//このサーバーの設定を保持する ServerConfig server_config;
	ServerConfig server_config;

	//"}" が来るまでループ
	while(!HttpConfigParser::isEof(tokens, index)) {
		std::string token = HttpConfigParser::getNextToken(tokens, index);
		if (token == "}") {
			//server ブロック終了
			break;
		}
		
		if(token == "location"){
			//"location"を見つけたら、location専門家を呼び出す。
			HttpConfigParser::parserLocation(server_config, tokens, index);
		}
		else if (token == "listen") {
			// listen 専門家を呼び出し、結果をserver_configにセット
			ListenDirective ld = HttpConfigParser::parseListen(tokens, index);
			server_config.setListen(ld); //<- セッターが必要
		}
		else if (token == "root") {
			// root 専門家を呼び出し、結果をserver_configにセット
			std::string r = HttpConfigParser::parseRoot(tokens, index);
			server_config.setRoot(r); //<- セッターが必要
		}
		else if (token == "index") {
			// index 専門家を呼び出し、結果をserver_configにセット
			std::vector<std::string> files = HttpConfigParser::parseIndex(tokens, index);
			for (size_t i = 0; i < files.size(); ++i) {
				server_config.addIndexFile(files[i]); //<- セッターが必要
			}
		}
		else if (token == "autoindex") {
			// autoindex 専門家を呼び出し、結果をserver_configにセット
			bool ai = HttpConfigParser::parseAutoindex(tokens, index);
			server_config.setAutoindex(ai); //<- セッターが必要
		}
		//ほかにもclient_max_body_size, error_page, return,などのディレクティブのパース処理を追加予定
		else {
			//知らないディレクティブはセミコロンまでスキップ
			while (HttpConfigParser::getNextToken(tokens, index) != ";") {
				//;を見つけるまで進める
				if (HttpConfigParser::isEof(tokens, index)) {
					throw std::runtime_error("Error: Expected ';' to terminate directive");
				}
				
			}
		}
	}

	//完成した server_config を config に追加//// (※ HttpConfig.h に public な addServer(ServerConfig s) セッターが必要)
	config.addServerConfig(server_config);
}


//-----------------------------------------------------------
//------------------ロケーションブロックパーサー----------------
//-----------------------------------------------------------

///@brief location ブロックをパースする
///@param server_config ServerConfig オブジェクト (参照渡し)
///@param tokens トークンリスト
///@param index 現在の位置 (参照渡しで、この値が内部で進められる)

void HttpConfigParser::parserLocation(ServerConfig& server_config, const std::vector<std::string>& tokens, size_t& index){

	//"location"の後にパスが来るはず
	// 次は、"パス" (例: "/" や "/images")
	std::string path = HttpConfigParser::getNextToken(tokens, index);
	
	//その次に '{' が来るはず
	if(HttpConfigParser::getNextToken(tokens, index) != "{") {
		throw std::runtime_error("Error: Expected '{' after location path" + path);
	}

	//このロケーションの設定を保持する LocationConfig location_config(path);
	LocationConfig location_config;

	// (※ LocationConfig.h に public な setPath(std::string s) セッターが必要です)
	location_config.setPath(path);

	//"}" が来るまでループ
	while(!HttpConfigParser::isEof(tokens, index)) {
		std::string token = HttpConfigParser::getNextToken(tokens, index);
		if (token == "}") {
			//location ブロック終了
			break;
		}
		if (token == "root") {
			// root 専門家を呼び出し、結果をlocation_configに
			std::string r = HttpConfigParser::parseRoot(tokens, index);
			location_config.setRoot(r); //<- セッターが必要
		}
		else if (token == "index") {
			// index 専門家を呼び出し、結果をlocation_configに
			std::vector<std::string> files = HttpConfigParser::parseIndex(tokens, index);
			for (size_t i = 0; i < files.size(); ++i) {
				location_config.addIndexFile(files[i]); //<- セッターが必要
			}
		}
		else if (token == "autoindex") {
			// autoindex 専門家を呼び出し、結果をlocation_configに
			bool ai = HttpConfigParser::parseAutoindex(tokens, index);
			location_config.setAutoindex(ai); //<- セッターが必要
		}
		//ほかにもclient_max_body_size, error_page, return,などのディレクティブのパース処理を追加予定
			//ここにroot, index, autoindexなどのディレクティブのパース処理を追加予定
		else {
			//知らないディレクティブはセミコロンまでスキップ
			while (HttpConfigParser::getNextToken(tokens, index) != ";") {
				//;を見つけるまで進める
				if (HttpConfigParser::isEof(tokens, index)) {
					throw std::runtime_error("Error: Expected ';' to terminate directive");
				}
			}
		}
	}
		// 6. 完成した location_config を、引数の server_config に追加する
    // (※ ServerConfig.h に public な addLocation(LocationConfig l) セッターが必要です)
    server_config.addLocation(location_config);
}

///@brief root ディレクティブをパースする
//@return root のパス　"/var/www/html"など
std::string HttpConfigParser::parseRoot(const std::vector<std::string>& tokens, size_t& index) {
	//root の後にパスが来るはず
	std::string path = HttpConfigParser::getNextToken(tokens, index);
	//セミコロンを期待
	if (HttpConfigParser::getNextToken(tokens, index) != ";") {
		throw std::runtime_error("Error: Expected ';' after root directive");
	}
	return path;
}

///@brief "autoindex" ディレクティブをパースする
//@return autoindex のオンオフ(true/false)
bool HttpConfigParser::parseAutoindex(const std::vector<std::string>& tokens, size_t& index) {
	//autoindex の後に "on" か "off"が来るはず
	std::string value = HttpConfigParser::getNextToken(tokens, index);
	
	if(HttpConfigParser::getNextToken(tokens, index) != ";") {
		throw std::runtime_error("Error: Expected ';' after autoindex directive");
	}

	if(value == "on") {
		return true;
	} else if (value == "off") {
		return false;
	} else {
		throw std::runtime_error("Error: autoindex value must be 'on' or 'off'");
	}
}

///@brief index ディレクティブをパースする (複数形対応)
//@return index ファイル名のリスト""index.html"など
std::vector<std::string> HttpConfigParser::parseIndex(const std::vector<std::string>& tokens, size_t& index) {
	std::vector<std::string> index_files;

	// ";" が来るまでループ
	while(!HttpConfigParser::isEof(tokens, index)) {
		std::string token = HttpConfigParser::getNextToken(tokens, index);
		if (token == ";") {
			//index ディレクティブ終了
			break;
		}
		index_files.push_back(token);
	}

	//breakでループを抜けた場合、getNextTokenが";"を消費しているので問題なし
	// もしEOFに達した場合はエラー

	//１つもファイル名がない場合はエラー
	if (index_files.empty()) {
		throw std::runtime_error("Error: Expected at least one index file before ';'");
	}

	return index_files;
}


///@brief listen ディレクティブをパースする
///@return 完成した ListenDirective オブジェクト
ListenDirective HttpConfigParser::parseListen(const std::vector<std::string>& tokens, size_t& index) {
	ListenDirective ld; //返すための構造体

	// listen の次のトークン取得
	std::string value = HttpConfigParser::getNextToken(tokens, index);

	//本当はlocalhostやドメイン名も解決したいが、とりあえずIPアドレスかポート番号だけ対応★

	// 形式を簡易的にチェック(address:port か port)
	std::string::size_type colon_pos = value.find(':');

	if(colon_pos != std::string::npos) {
		// address:port 形式
		ld.address = value.substr(0, colon_pos);
		std::string port_str = value.substr(colon_pos + 1);

		//C++98で文字列を数値に変換(sstreamを使用)
		std::stringstream ss(port_str);
		if(!(ss >> ld.port) || !ss.eof()) {//変換失敗or あとにごみがある場合
			throw std::runtime_error("Error: Invalid port number +" + port_str);
		}
	} else {
		// port 形式のみ
		ld.address = "0.0.0.0"; //デフォルトのアドレス

		std::stringstream ss(value);
		if(!(ss >> ld.port) || !ss.eof()) {
			throw std::runtime_error("Error: Invalid port number +" + value);
		}
	}

	// 次のトークンを確認して、"default_server" かどうかチェック
	std::string next_token = HttpConfigParser::getNextToken(tokens, index);
	if (next_token == "default_server") {
		ld.is_default_server = true;
		//さらにもう一つトークンをよみ、セミコロンを期待
		next_token = HttpConfigParser::getNextToken(tokens, index);
	}


	//最後はセミコロンを期待
	if (next_token != ";") {
		throw std::runtime_error("Error: Expected ';' after listen directive");
	}

	return ld;
}
