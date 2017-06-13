<?php
  
  //载入公共配置
  $config	= require __PUBLIC__  . '/config.inc.php';
  
  //设定项目配置
  $array = array(

    // 每页显示记录数
    'PAGE_PER' => 10,
    
    // 关闭自动Session
    'SESSION_AUTO_START'=>false,
    'SHOW_PAGE_TRACE'=>false,

    //'URL_MODEL'=>3,
    'URL_ROUTER_ON'=>true,
    
    //'SITE_URL' =>	'http://www.myhaoyi.com',
    
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
