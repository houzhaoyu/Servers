#include "MsgNode.h"
RecvNode::RecvNode(int max_len, short msg_id):MsgNode(max_len),
_msg_id(msg_id){

}


SendNode::SendNode(const char* msg, int max_len, short msg_id):MsgNode(max_len + FILE_HEAD_TOTAL_LEN)
, _msg_id(msg_id){
	//先发送id, 转为网络字节序
	short msg_id_host = boost::asio::detail::socket_ops::host_to_network_short(msg_id);
	memcpy(_data, &msg_id_host, HEAD_ID_LEN);
	//转为网络字节序
	int max_len_host = boost::asio::detail::socket_ops::host_to_network_long(max_len);
	memcpy(_data + HEAD_ID_LEN, &max_len_host, FILE_HEAD_DATA_LEN);
	memcpy(_data + HEAD_ID_LEN + FILE_HEAD_DATA_LEN, msg, max_len);
}
