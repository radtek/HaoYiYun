<?php
/*************************************************************
    HaoYi (C)2017 - 2020 myhaoyi.com
*************************************************************/

// 定义日志存储函数
function logdebug($text)
{
  file_put_contents(WK_ROOT . '/logwechat.txt', $text."\n", FILE_APPEND);
}

// 定义一些需要的全局变量...
define('NAV_ITEM_COUNT',      6);	  // 导航栏 - 条目个数...
define('NAV_ACTIVE_HOME',     0);    // 导航栏 - 首页激活... 
define('NAV_ACTIVE_LIVE',     1);    // 导航栏 - 直播激活...
define('NAV_ACTIVE_SUBJECT',  2);    // 导航栏 - 科目激活...
define('NAV_ACTIVE_MORE',     3);    // 导航栏 - 更多激活...

// 定义重复类型
define('kNoneRepeat',         0);    // 无
define('kDayRepeat',          1);    // 每天
define('kWeekRepeat',         2);    // 每周

// 定义Transmit中转服务器支持的客户端类型...
define('kClientPHP',          1);
define('kClientGather',       2);
define('kClientLive',         3);  // RTMPAction
define('kClientPlay',         4);  // RTMPAction

// 定义Transmit中转服务器反馈错误号...
define('ERR_OK',                      0);
define('ERR_NO_SOCK',             10001);
define('ERR_NO_GATHER',           10002);
define('ERR_SOCK_SEND',           10003);
define('ERR_NO_JSON',             10004);
define('ERR_NO_COMMAND',          10005);
define('ERR_NO_RTMP',             10006);
define('ERR_NO_MAC_ADDR',         10007);

// 定义Transmit服务器可以执行的命令列表...
define('kCmd_Gather_Login',           1);
define('kCmd_PHP_Get_Camera_Status',  2);
define('kCmd_PHP_Set_Camera_Name',    3);
define('kCmd_PHP_Set_Course_Add',     4);
define('kCmd_PHP_Set_Course_Mod',     5);
define('kCmd_PHP_Set_Course_Del',     6);
define('kCmd_PHP_Get_Gather_Status',  7);
define('kCmd_PHP_Get_Course_Record',  8);
define('kCmd_Live_Login',             9);
define('kCmd_Live_Vary',             10);
define('kCmd_Play_Login',            11);

?>