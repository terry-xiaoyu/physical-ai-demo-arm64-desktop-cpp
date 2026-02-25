#pragma once

#include <string>

/**
 * VolcEngineRTC 常量定义
 *
 * - APPID 使用SDK前需要为自己的应用申请一个AppId，详情参见{https://www.volcengine.com/docs/6348/69865}
 * - APPKEY 
 * - TOKEN 加入房间的时候需要使用token完成鉴权，详情参见{https://www.volcengine.com/docs/6348/70121} 
 *   TOKEN 由 APPID, APPKEY, 房间ID, 用户ID 决定。此 Demo 中使用的 TOKEN 是本地生成的。实际使用请从远端生成。
 * - INPUT_REGEX SDK 对房间名、用户名的限制是：非空且最大长度不超过128位的数字、大小写字母、@ . _ \ -
 */
class Constants
{
public:
	 static std::string APP_ID;
	 static std::string APP_KEY;
	 static std::string INPUT_REGEX;
};

std::string Constants::APP_ID = "eeece";

std::string Constants::APP_KEY = "eecce9908";

std::string Constants::INPUT_REGEX = "^[a-zA-Z0-9@._-]{1,128}$";