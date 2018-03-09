﻿<?php
  
  //载入公共配置
  $config	= require __PUBLIC__  . '/config.inc.php';
  
  //设定项目配置
  $array = array(
    // 微信登录网站应用需要的参数配置 => 微信开放平台
    'WECHAT_LOGIN' => array(
      'scope'=>'snsapi_login', //用户授权的作用域,使用逗号(,)分隔
      'appid'=>'wxc1d8a43197ccc500', //网站登录应用的appid
      'appsecret'=>'c5a1eb36e79cc710f1f73a6ee88e469a', //网站登录应用的appsecret
      'redirect_uri'=>'http://www.myhaoyi.com/', //回调地址
    ),
    // 微信小程序参数配置 => 浩一云 => 直播播放...
    'CLOUD_MINI' => array(
      'appid'=>'wx78b419f717fb1552',
      'appsecret'=>'29605607d5005fa5f1a5dde59eeb42eb'
    ), 
    // 微信小程序参数配置 => 浩一云服务 => 只有设备管理...
    'DEVICE_MINI' => array(
      'appid'=>'wxf01ed931c3ece5e2',
      'appsecret'=>'55e1cf6fc18544692e062b8998b31cc4'
    ), 
    // 每页显示记录数
    'PAGE_PER' => 10,
    
    // 关闭自动Session
    'SESSION_AUTO_START'=>false,
    'SHOW_PAGE_TRACE'=>false,
    
    //'URL_MODEL'=>3,
    'URL_ROUTER_ON'=>true,
    
    'TMPL_TEMPLATE_SUFFIX'=>'.htm',
    'TMPL_CACHE_TIME'=>0,
    'TMPL_L_DELIM'=>'{{',
    'TMPL_R_DELIM'=>'}}',
    'DATA_CACHE_SUBDIR'=>true,
    'DATA_PATH_LEVEL'=>2,
    
    'LANG_SWITCH_ON'=>true,
    'LANG_AUTO_DETECT'=>false,
    'DEFAULT_LANG'=>'zh-cn',
    'DB_FIELDTYPE_CHECK'=>true,
    
    //定义保留关键字...
    'DIFNAME'=>array('admin','home','api','client','index','pub','v','m','p','setting','dologin','login','logout','register','regcheck','reset','doreset','checkreset','setpass','space','message','find','topic','hot','index','widget','comments','wap','map','plugins','url','guide','sendapi','blacklist','vote'),
  );
  
  //合并输出配置
  return array_merge($config,$array);
?>
