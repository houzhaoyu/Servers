#include "StatusServiceImpl.h"
#include "Logger.h"
#include "RedisMgr.h"
#include "const.h"

#include <string>

namespace
{
	std::string GenerateUniqueString()
	{
		return boost::uuids::to_string(boost::uuids::random_generator()());
	}

	// 一次 Redis 往返完成活跃节点过滤、最小连接数选择和同负载轮询。
	const char *kSelectServerScript = R"lua(
local names = redis.call('SMEMBERS', KEYS[1])
local best = {}
local min_count = nil
for _, name in ipairs(names) do
    local info_key = ARGV[1] .. name
    if redis.call('EXISTS', info_key) == 1 then
        local count = tonumber(redis.call('HGET', KEYS[2], name) or '0')
        if min_count == nil or count < min_count then
            min_count = count
            best = {name}
        elseif count == min_count then
            table.insert(best, name)
        end
    end
end
if #best == 0 then
    return {}
end
local sequence = redis.call('INCR', ARGV[2])
local name = best[((sequence - 1) % #best) + 1]
local info_key = ARGV[1] .. name
return {name,
        redis.call('HGET', info_key, 'host') or '',
        redis.call('HGET', info_key, 'port') or '',
        tostring(min_count)}
)lua";
}

grpc::ServerUnaryReactor *StatusServiceImpl::GetChatServer(
	grpc::CallbackServerContext *context,
	const message::GetChatServerReq *request,
	message::GetChatServerRsp *reply)
{
	auto *reactor = context->DefaultReactor();
	const auto uid = request->uid();
	RedisMgr::GetInstance()->AsyncCommand(
		{ "EVAL", kSelectServerScript, "2", CHATSERVER_REGISTRY, LOGIN_COUNT,
		  SERVER_INFO_PREFIX, "status_server_round_robin" },
		[reactor, reply, uid](RedisAsyncResult result)
		{
			if (!result.success || result.elements.size() < 3)
			{
				Logger::Error("async Redis failed to select chat server: {}", result.error);
				reply->set_error(ErrorCodes::RPCFailed);
				reactor->Finish(grpc::Status::OK);
				return;
			}

			reply->set_host(result.elements[1]);
			reply->set_port(result.elements[2]);
			reply->set_token(GenerateUniqueString());
			reply->set_error(ErrorCodes::Success);

			const auto token_key = std::string(USER_TOKEN_PREFIX) + std::to_string(uid);
			RedisMgr::GetInstance()->AsyncSet(token_key, reply->token(),
				[reactor](RedisAsyncResult set_result)
				{
					if (!set_result.success)
					{
						Logger::Error("async Redis failed to insert login token: {}", set_result.error);
					}
					reactor->Finish(grpc::Status::OK);
				});
		});
	return reactor;
}

grpc::ServerUnaryReactor *StatusServiceImpl::Login(
	grpc::CallbackServerContext *context,
	const message::LoginReq *request,
	message::LoginRsp *reply)
{
	auto *reactor = context->DefaultReactor();
	const auto uid = request->uid();
	const auto token = request->token();
	const auto token_key = std::string(USER_TOKEN_PREFIX) + std::to_string(uid);
	RedisMgr::GetInstance()->AsyncGet(token_key,
		[reactor, reply, uid, token](RedisAsyncResult result)
		{
			if (!result.success)
			{
				reply->set_error(ErrorCodes::UidInvalid);
			}
			else if (result.value != token)
			{
				reply->set_error(ErrorCodes::TokenInvalid);
			}
			else
			{
				reply->set_error(ErrorCodes::Success);
				reply->set_uid(uid);
				reply->set_token(token);
			}
			reactor->Finish(grpc::Status::OK);
		});
	return reactor;
}
