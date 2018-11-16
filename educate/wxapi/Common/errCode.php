<?php
/*************************************************************
    HaoYi (C)2017 - 2020 myhaoyi.com
*************************************************************/

// 定义日志存储函数
function logdebug($text)
{
  file_put_contents(WK_ROOT . '/logwechat.txt', $text."\n", FILE_APPEND);
}

// 定义整个系统模式类型...
define('kCloudRecorder',      0);   // 云录播...
define('kCloudMonitor',       1);   // 云监控...
define('kCloudEducate',       2);   // 云教室...

define('LIVE_BEGIN_ID',  200000);   // 直播间开始编号...

// 定义性别类型标识符号...
define('kNoneSex',           0);
define('kMaleSex',           1);
define('kFeMaleSex',         2);
// 定义性别名称数组，需要使用eval才能返回数组...
define('SEX_TYPE', "return array('无', '男', '女');");

// 定义用户类型标识符号...
define('kParentUser',        0);    // 家长
define('kAssistUser',        1);    // 助教
define('kTeacherUser',       2);    // 讲师
define('kShopKeeperUser',    3);    // 店长
define('kShopOwnerUser',     4);    // 门店老板
define('kMaintainUser',      5);    // 运营维护
define('kAdministerUser',    6);    // 系统管理员
// 定义用户类型名称数组，需要使用eval才能返回数组...
define('USER_TYPE', "return array('家长', '助教', '讲师', '店长', '门店老板', '运营维护', '系统管理员');");

// 定义父母类型标识符号...
define('kNoneParent',        0);     // 无
define('kMamaParent',        1);     // 妈妈
define('kPapaParent',        2);     // 爸爸
define('kFamiParent',        3);     // 亲属
// 定义父母类型名称数组，需要使用eval才能返回数组...
define('PARENT_TYPE', "return array('无', '妈妈', '爸爸', '亲属');");

// 定义一些需要的全局变量...
define('NAV_ITEM_COUNT',      6);	  // 导航栏 - 条目个数...
define('NAV_ACTIVE_HOME',     0);    // 导航栏 - 首页激活... 
define('NAV_ACTIVE_LIVE',     1);    // 导航栏 - 直播激活...
define('NAV_ACTIVE_TOUR',     2);    // 导航栏 - 巡课激活...
define('NAV_ACTIVE_SUBJECT',  3);    // 导航栏 - 科目激活...
define('NAV_ACTIVE_MORE',     4);    // 导航栏 - 更多激活...
define('NAV_ACTIVE_LOGIN',    5);    // 导航栏 - 登录激活...

// 定义重复类型
define('kNoneRepeat',         0);    // 无
define('kDayRepeat',          1);    // 每天
define('kWeekRepeat',         2);    // 每周

// 定义UDP中转服务器支持的客户端类型...
define('kClientPHP',          1);
define('kClientStudent',      2);
define('kClientTeacher',      3);
define('kClientUdpServer',    4);

// 定义UDP中转服务器反馈错误号...
define('ERR_OK',                      0);
define('ERR_NO_ROOM',             10001);
define('ERR_NO_SERVER',           10002);

// 定义UDP服务器可以执行的命令列表...
define('kCmd_Student_Login',          1);
define('kCmd_Student_OnLine',         2);
define('kCmd_Teacher_Login',          3);
define('kCmd_Teacher_OnLine',         4);
define('kCmd_UDP_Logout',             5);
define('kCmd_Camera_PullStart',       6);
define('kCmd_Camera_PullStop',        7);
define('kCmd_Camera_OnLineList',      8);
define('kCmd_Camera_LiveStart',       9);
define('kCmd_Camera_LiveStop',       10);
define('kCmd_UdpServer_Login',       11);
define('kCmd_UdpServer_OnLine',      12);
define('kCmd_UdpServer_AddTeacher',  13);
define('kCmd_UdpServer_DelTeacher',  14);
define('kCmd_UdpServer_AddStudent',  15);
define('kCmd_UdpServer_DelStudent',  16);
define('kCmd_PHP_GetUdpServer',      17);
define('kCmd_PHP_GetAllServer',      18);
define('kCmd_PHP_GetAllClient',      19);
define('kCmd_PHP_GetRoomList',       20);
define('kCmd_PHP_GetPlayerList',     21);

//////////////////////////////////////////////////////
// 定义一组通用的公用函数列表...
//////////////////////////////////////////////////////

//
// 去掉最后一个字符，如果是反斜杠...
function removeSlash($strUrl)
{
  $nSize = strlen($strUrl) - 1;
  // 去掉最后的反斜杠字符...
  if( $strUrl{$nSize} == '/' ) {
    $strUrl = substr($strUrl, 0, $nSize);
  }
  return $strUrl;
}
//
// base64编码处理...
function urlsafe_b64encode($string)
{
  $data = base64_encode($string);
  $data = str_replace(array('+','/','='),array('-','_',''),$data);
  return $data;
}
//
// base64解码处理...
function urlsafe_b64decode($string)
{
  $data = str_replace(array('-','_'),array('+','/'),$string);
  $mod4 = strlen($data) % 4;
  if($mod4) {
    $data .= substr('====', $mod4);
  }
  return base64_decode($data);
}  
/**
* GET 请求
* @param string $url
*/
function http_get( $url )
{
  $oCurl = curl_init();
  if(stripos($url,"https://")!==FALSE) {
    curl_setopt($oCurl, CURLOPT_SSL_VERIFYPEER, FALSE);
    curl_setopt($oCurl, CURLOPT_SSL_VERIFYHOST, FALSE);
    curl_setopt($oCurl, CURLOPT_SSLVERSION, 1); //CURL_SSLVERSION_TLSv1
  }
  curl_setopt($ch, CURLOPT_TIMEOUT, 3);
  curl_setopt($oCurl, CURLOPT_URL, $url);
  curl_setopt($oCurl, CURLOPT_RETURNTRANSFER, 1 );
	// 2017.06.06 => resolve DNS slow problem...
	curl_setopt($oCurl, CURLOPT_HTTPHEADER, array("Expect: ")); 
	curl_setopt($oCurl, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4 );
	curl_setopt($oCurl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_0);
  $sContent = curl_exec($oCurl);
  $aStatus = curl_getinfo($oCurl);
  curl_close($oCurl);
  if(intval($aStatus["http_code"])==200){
    return $sContent;
  }else{
    return false;
  }
}
/**
 * POST 请求
 * @param string $url
 * @param array $param
 * @param boolean $post_file 是否文件上传
 * @return string content
 */
function http_post($url,$param,$post_file=false)
{
	$oCurl = curl_init();
	if(stripos($url,"https://")!==FALSE){
		curl_setopt($oCurl, CURLOPT_SSL_VERIFYPEER, FALSE);
		curl_setopt($oCurl, CURLOPT_SSL_VERIFYHOST, false);
		curl_setopt($oCurl, CURLOPT_SSLVERSION, 1); //CURL_SSLVERSION_TLSv1
	}
	if (is_string($param) || $post_file) {
		$strPOST = $param;
	} else {
		$aPOST = array();
		foreach($param as $key=>$val){
			$aPOST[] = $key."=".urlencode($val);
		}
		$strPOST =  join("&", $aPOST);
	}
	curl_setopt($oCurl, CURLOPT_URL, $url);
	curl_setopt($oCurl, CURLOPT_RETURNTRANSFER, 1 );
	curl_setopt($oCurl, CURLOPT_POST,true);
	curl_setopt($oCurl, CURLOPT_POSTFIELDS,$strPOST);
	$sContent = curl_exec($oCurl);
	$aStatus = curl_getinfo($oCurl);
	curl_close($oCurl);
	if(intval($aStatus["http_code"])==200){
		return $sContent;
	}else{
		return false;
	}
}
/**
* 作用：产生随机字符串，不长于32位
*/
// 生成一个不超过32位的state...
//$state = $this->createNoncestr();
function createNoncestr( $length = 32 ) 
{
  $chars = "abcdefghijklmnopqrstuvwxyz0123456789";  
  $str ="";
  for ( $i = 0; $i < $length; $i++ )  {  
    $str.= substr($chars, mt_rand(0, strlen($chars)-1), 1);  
  }
  return $str;
}

// 获取转发错误信息...
function getTransmitErrMsg($inErrCode)
{
  switch( $inErrCode )
  {
    case ERR_OK:          $strErrMsg = 'ok'; break;
    case ERR_NO_ROOM:     $strErrMsg = '没有房间号。'; break;
    case ERR_NO_SERVER:   $strErrMsg = '没有在线的直播服务器。'; break;
    default:              $strErrMsg = '未知错误，请确认中转服务器版本。'; break;
  }
  return $strErrMsg;
}
// 代替扩展插件的，直接用socket实现的与中转服务器通信函数...
/*function php_transmit_command($inIPAddr, $inIPPort, $inClientType, $inCmdID, $inSaveJson = false)
{
  // 创建并连接服务器...
  $socket = socket_create(AF_INET, SOCK_STREAM, SOL_TCP);
  if( !$socket ) return false;
  // 链接指定的服务器和端口...
  if( !socket_connect($socket, $inIPAddr, $inIPPort) ) {
    socket_close($socket);
    return false;
  }
  // 组合发送数据包 => m_pkg_len | m_type | m_cmd | m_sock
  $pkg_len = (($inSaveJson) ? strlen($inSaveJson) : 0);
  $strData = pack('LLLL', $pkg_len, $inClientType, $inCmdID, 0);
  // 计算带有数据包的缓存内容...
  $strData = (($inSaveJson) ? ($strData . $inSaveJson) : $strData);
  // 发送已经准备好的数据包...
  if( !socket_write($socket, $strData, strlen($strData)) ) {
    socket_close($socket);
    return false;
  }
  // 接收服务器传递过来的数据包...
  $strRecv = socket_read($socket, 8192);
  if( !$strRecv ) {
    socket_close($socket);
    return false;
  }
  // 去掉10字节的数据包...
  if( strlen($strRecv) > 10 ) {
    $strJson = substr($strRecv, 10, strlen($strRecv) - 10);
  }
  // 关闭连接，返回json数据包..
  socket_close($socket);
  return $strJson;
}*/

?>