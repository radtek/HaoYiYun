<?php
/*************************************************************
    HaoYi (C)2017 - 2020 myhaoyi.com

*************************************************************/

class MonitorAction extends Action
{
  public function _initialize()
  {
    // 加载 ThinkPHP 的扩展函数 => ThinkPHP/Common/extend.php => msubstr()
    Load('extend');
    // 获取系统配置，根据配置设置相关变量 => 强制配置成云监控...
    $dbSys = D('system')->field('web_type,web_title,sys_site')->find();
    $this->m_webTitle = $dbSys['web_title'];
    $this->m_sysSite = $dbSys['sys_site'];
    $this->m_webType = kCloudMonitor;
    // 创建一个新的移动检测对象...
    $this->m_detect = new Mobile_Detect();
    //////////////////////////////////////////////
    // 移动端访问了电脑端页面，直接对页面进行跳转...
    //////////////////////////////////////////////
    if( $this->m_detect->isMobile() ) {
      header("location: /Mobile");
    }
    // 判断是否是IE浏览器 => 这里必须用0和1表示...
    $this->m_isIEBrowser = 0;
    $theIERule = '/(MSIE) (\d+\.\d)/';
    $theUserAgent = $this->m_detect->getUserAgent();
    // 通过正则匹配，是否是IE浏览器...
    preg_match($theIERule, $theUserAgent, $data);
    // IE11 以下禁用高级flvjs和hls，只能用flash播放...
    if( !empty($data) && is_array($data) ) {
      $this->m_isIEBrowser = (($data[2] >= '11.0') ? 0 : 1);
    }
    // 如果是监控模式，设置常量信息...
    $this->m_webAction = "Monitor";
    // 直接给模板变量赋值...
    $this->assign('my_web_action', $this->m_webAction);
    $this->assign('my_sys_site', $this->m_sysSite);
    $this->assign('my_web_title', $this->m_webTitle);
  }
  //
  // 点击查看用户登录信息...
  public function userInfo()
  {
    // 判断用户是否已经处于登录状态，直接进行页面跳转...
    if( !Cookie::is_set('wx_unionid') || !Cookie::is_set('wx_headurl') || !Cookie::is_set('wx_ticker') ) {
      echo "<script>window.parent.doReload();</script>";
      return;
    }
    // 调用Admin->userInfo接口...
    A('Admin')->userInfo();
  }
  //
  // 点击注销导航页...
  public function logout()
  {
    ////////////////////////////////////////////////////////////////////
    //注意：不能用Cookie::delete()无法彻底删除，它把空串进行了base64编码...
    ////////////////////////////////////////////////////////////////////

    // 删除存放的用户cookie信息...
    setcookie('wx_unionid','',-1,'/');
    setcookie('wx_headurl','',-1,'/');
    setcookie('wx_ticker','',-1,'/');
    
    $my_nav['is_login'] = false;
    $this->assign('my_nav', $my_nav);
    $this->display('Common:monitor_login');
  }
  //
  // 点击登录导航页...
  // 参数：wx_error | wx_unionid | wx_headurl
  public function login()
  {
    A('Login')->login(false);
  }
  //
  // 得到导航数据...
  private function getNavData($activeType, $activeID)
  {
    // 根据cookie设定 登录|注销 导航栏...
    if( Cookie::is_set('wx_unionid') && Cookie::is_set('wx_headurl') && Cookie::is_set('wx_ticker') ) {
      $my_nav['is_login'] = true;
      $my_nav['headurl'] = Cookie::get('wx_headurl');
      // 判断当前页面是否是https协议 => 通过$_SERVER['HTTPS']和$_SERVER['REQUEST_SCHEME']来判断...
      if((isset($_SERVER['HTTPS']) && $_SERVER['HTTPS'] == 'on') || (isset($_SERVER['REQUEST_SCHEME']) && $_SERVER['REQUEST_SCHEME'] == 'https')) {
        $my_nav['headurl'] = str_replace('http://', 'https://', $my_nav['headurl']);
      }
    } else {
      $my_nav['is_login'] = false;
    }
    // 获取采集器列表...
    $arrDrop = D('gather')->order('gather_id ASC')->select();
    // 设置一些返回数据...
    $my_nav['type'] = $activeType;          // 0(首页激活), 1(实时激活), 2(采集器激活), 3(更多激活)
    $my_nav['active_id'] = $activeID;       // 激活的采集器编号...
    $my_nav['drop_more'] = "更多";          // 默认的下拉名称...
    // 如果是采集器状态，需要进一步的判断...
    if( $activeType == NAV_ACTIVE_SUBJECT ) {
      // 遍历采集器列表，设置下拉名称...
      $bFindActive = false;
      foreach($arrDrop as $key => &$dbItem) {
        if( $dbItem['gather_id'] == $my_nav['active_id'] ) {
          $my_nav['subject_title'] = $dbItem['name_set'];
          $bFindActive = ((($key + 1) > NAV_ITEM_COUNT) ? true : false);
          break;
        }
      }
      // 如果是下拉状态，设置下拉名称...
      if( isset($_GET['more']) || $bFindActive ) {
        $my_nav['type'] = NAV_ACTIVE_MORE;
        $my_nav['drop_more'] = $my_nav['subject_title'];
      }
    }
    // 将获取的采集器数组进行分割 => 5个采集器
    $arrList = array_splice($arrDrop, 0, NAV_ITEM_COUNT - 1);
    // 将分割后的数据进行赋值...
    $my_nav['list'] = $arrList;             // 横向采集器列表...
    $my_nav['drop'] = $arrDrop;             // 下拉采集器列表...
    $my_nav['drop_num'] = count($arrDrop); // 下拉采集器数量...
    // 直接返回计算之后的导航数据...
    return $my_nav;
  }
  //
  // 默认首页...
  public function index()
  {
    // 获取首页导航数据...
    $my_nav = $this->getNavData(NAV_ACTIVE_HOME, 0);
    $this->assign('my_title', $this->m_webTitle . ' - 首页');
    $this->assign('my_nav', $my_nav);
    // 计算右侧，最近更新，前10条...
    //$arrNews = D('RecordView')->limit(10)->order('Record.created DESC')->select();
    $arrNews = D('record')->limit(10)->order('created DESC')->select();
    $this->assign('my_news', $arrNews);
    //计算右侧，点击排行，前10条...
    //$arrList = D('RecordView')->limit(10)->order('Record.clicks DESC')->select();
    $arrList = D('record')->limit(10)->order('clicks DESC')->select();
    $this->assign('my_clicks', $arrList);
    // 得到每页通道数，总记录数，计算总页数...
    $pagePer = C('PAGE_PER');
    $totalNum = D('camera')->count();
    $max_page = intval($totalNum / $pagePer);
    // 判断是否是整数倍的页码...
    $max_page += (($totalNum % $pagePer) ? 1 : 0);
    // 设置最大页数，设置模板参数...
    $this->assign('max_page', $max_page);
    $this->display('index');
  }
  //
  // 首页流加载...
  public function pageIndex()
  {
    // 准备需要的分页参数...
    $pagePer = C('PAGE_PER'); // 每页显示的通道数...
    $pageCur = (isset($_GET['p']) ? $_GET['p'] : 1);  // 当前页码...
    $pageLimit = (($pageCur-1)*$pagePer).','.$pagePer; // 读取范围...
    // 读取通道下面的录像列表，只读取前8条记录...
    $arrList = D('camera')->limit($pageLimit)->field('camera_id,gather_id,camera_name')->order('camera_id ASC')->select();
    foreach($arrList as &$dbItem) {
      $map['Record.camera_id'] = $dbItem['camera_id'];
      $theName = "live" . $dbItem['camera_id'];
      $arrRec[$theName]['id'] = $dbItem['camera_id'];
      $arrRec[$theName]['name'] = $dbItem['camera_name'];
      $arrRec[$theName]['data'] = D('RecordView')->where($map)->limit(8)->order('created DESC')->select();
    }
    // 对模板对象进行赋值 => web_tracker_addr 已经自带了协议头 http://或https://
    $dbSys = D('system')->field('web_tracker_addr,web_tracker_port')->find();
    $this->assign('my_web_tracker', sprintf("%s:%d/", $dbSys['web_tracker_addr'], $dbSys['web_tracker_port']));
    $this->assign('my_cur_page', $pageCur);
    $this->assign('my_rec', $arrRec);
    echo $this->fetch('pageIndex');
  }
  //
  // 点击实时页面...
  public function live()
  {
    // 获取直播导航数据...
    $my_nav = $this->getNavData(NAV_ACTIVE_LIVE, 0);
    // 对模板对象进行赋值...
    $this->assign('my_title', $this->m_webTitle . ' - 实时');
    $this->assign('my_nav', $my_nav);
    // 得到每页通道数，总记录数，计算总页数...
    $pagePer = 16; //C('PAGE_PER');
    $totalNum = D('camera')->count();
    $max_page = intval($totalNum / $pagePer);
    // 判断是否是整数倍的页码...
    $max_page += (($totalNum % $pagePer) ? 1 : 0);
    // 设置最大页数，设置模板参数...
    $onlineNum = D('camera')->where('status > 0')->count();
    $this->assign('my_online_num', $onlineNum);
    $this->assign('my_total_num', $totalNum);
    $this->assign('max_page', $max_page);
    // 设置模板参数...
    $this->display('live');    
  }
  //
  // 实时页面流加载...
  public function pageLive()
  {
    // 准备需要的分页参数...
    $pagePer = 16; //C('PAGE_PER'); // 每页显示的通道数...
    $pageCur = (isset($_GET['p']) ? $_GET['p'] : 1);  // 当前页码...
    $pageLimit = (($pageCur-1)*$pagePer).','.$pagePer; // 读取范围...
    // 读取通道列表 => 在线优先排序
    $arrCamera = D('LiveView')->limit($pageLimit)->order('status DESC, updated DESC')->select();
    // 设置其它模板参数 => web_tracker_addr 已经自带了协议头 http://或https://
    $dbSys = D('system')->field('web_tracker_addr,web_tracker_port')->find();
    $this->assign('my_web_tracker', sprintf("%s:%d/", $dbSys['web_tracker_addr'], $dbSys['web_tracker_port']));
    // 设置模板参数 => 使用 pageLive 的模版...
    $this->assign('my_cur_page', $pageCur);
    $this->assign('my_list', $arrCamera);
    $this->display('pageLive');
  }
  //
  // 点击采集器页面...
  public function gather()
  {
    // 获取采集器导航数据，如果是下拉状态，设置对应的标志信息...
    $gather_id = $_GET['id'];
    $my_nav = $this->getNavData(NAV_ACTIVE_SUBJECT, $gather_id);
    $my_nav['more'] = ($my_nav['type'] == NAV_ACTIVE_MORE) ? "more/1" : "";
    $this->assign('my_title', $this->m_webTitle . ' - ' . $my_nav['subject_title']);
    $this->assign('my_nav', $my_nav);
    // 得到每页通道数，总记录数，计算总页数...
    $pagePer = 16; //C('PAGE_PER');
    $map['gather_id'] = $gather_id;
    $totalNum = D('camera')->where($map)->count();
    $max_page = intval($totalNum / $pagePer);
    // 判断是否是整数倍的页码...
    $max_page += (($totalNum % $pagePer) ? 1 : 0);
    // 设置最大页数，设置模板参数...
    $map['status'] = array('gt', 0);
    $onlineNum = D('camera')->where($map)->count();
    $this->assign('my_gather_id', $gather_id);
    $this->assign('my_online_num', $onlineNum);
    $this->assign('my_total_num', $totalNum);
    $this->assign('max_page', $max_page);
    // 设置模板参数...
    $this->display('gather');
  }
  //
  // 采集器分页流加载...
  public function pageGather()
  {
    // 准备需要的分页参数...
    $pagePer = 16; //C('PAGE_PER'); // 每页显示的通道数...
    $pageCur = (isset($_GET['p']) ? $_GET['p'] : 1);  // 当前页码...
    $pageLimit = (($pageCur-1)*$pagePer).','.$pagePer; // 读取范围...
    // 读取通道列表 => 在线优先排序
    $map['gather_id'] = $_GET['gather_id'];
    $arrCamera = D('LiveView')->where($map)->limit($pageLimit)->order('status DESC, updated DESC')->select();
    // 设置其它模板参数 => web_tracker_addr 已经自带了协议头 http://或https://
    $dbSys = D('system')->field('web_tracker_addr,web_tracker_port')->find();
    $this->assign('my_web_tracker', sprintf("%s:%d/", $dbSys['web_tracker_addr'], $dbSys['web_tracker_port']));
    // 设置模板参数 => 使用 pageLive 的模版...
    $this->assign('my_cur_page', $pageCur);
    $this->assign('my_list', $arrCamera);
    $this->display('pageLive');
  }
  //
  // 播放页面...
  public function play()
  {
    // 计算web_tracker地址 => 已经自带了协议头 http://或https://
    $dbSys = D('system')->field('web_tracker_addr,web_tracker_port')->find();
    $theWebTracker = sprintf("%s:%d/", $dbSys['web_tracker_addr'], $dbSys['web_tracker_port']);
    // 获取通道详细信息...
    $theGoToSlideID = 0;
    $camera_id = $_GET['camera_id'];
    $map['camera_id'] = $camera_id;
    $dbCamera = D('LiveView')->where($map)->find();
    // 快照地址无效，使用默认的快照；快照有效，使用快照动态缩略图...
    if( strlen($dbCamera['image_fdfs']) <= 0 ) {
      $dbCamera['image_snap'] = "/wxapi/public/images/snap.png";
    } else {
      $dbCamera['image_snap'] = sprintf("%s%s", $theWebTracker, $dbCamera['image_fdfs']);
    }
    // 给通道模版赋值...
    $this->assign('my_camera', $dbCamera);
    // 右侧第一条记录一定是直播...
    unset($map); $arrList[0] = $dbCamera;
    // 获取播放页导航数据 => 内部会进行下来导航纠正...
    $gather_id = $dbCamera['gather_id'];
    $my_nav = $this->getNavData(NAV_ACTIVE_SUBJECT, $gather_id);
    // 根据传递参数判断是vod或live...
    $bIsVod = (isset($_GET['record_id']) ? 1 : 0);
    if( $bIsVod ) {
      // 如果是点播模式，先找到当前点播记录...
      $record_id = $_GET['record_id'];
      $map['Record.record_id'] = $record_id;
      $dbCurVod = D('VodView')->where($map)->find();
      $strCurDate = substr($dbCurVod['created'], 0, 10);
      $strStart = sprintf("%s 00:00:00", $strCurDate);
      $strEnd = sprintf("%s 23:59:59", $strCurDate);
      // 再找到当前记录同一天的点播列表，加入通道筛选条件...
      $where['Record.camera_id'] = $camera_id;
      $where['Record.created'] = array('BETWEEN', array($strStart, $strEnd));
      $arrVod = D('VodView')->where($where)->order('Record.created ASC')->select();
      $arrList = array_merge($arrList, $arrVod);
      // 找到跳转录像的索引编号...
      foreach($arrList as $key => &$dbItem) {
        if( isset($dbItem['record_id']) && $dbItem['record_id'] == $record_id ) {
          $theGoToSlideID = $key; break;
        }
      }
    } else {
      // 处理直播模式 => 如果没有日期参数，就用当前日期...
      $strCurDate = (isset($_GET['date']) ? $_GET['date'] : date('Y-m-d'));
      $strStart = sprintf("%s 00:00:00", $strCurDate);
      $strEnd = sprintf("%s 23:59:59", $strCurDate);
      // 找到同一天的点播列表，加入通道筛选条件...
      $where['Record.camera_id'] = $camera_id;
      $where['Record.created'] = array('BETWEEN', array($strStart, $strEnd));
      $arrVod = D('VodView')->where($where)->order('Record.created ASC')->select();
      // 查询到了录像数据才进行合并，否则会出错...
      if( count($arrVod) > 0 ) {
        $arrList = array_merge($arrList, $arrVod);
      }
      // 有筛选日期，则查看录像记录，录像记录大于0，使用第一条录像...
      if( isset($_GET['date']) ) {
        $theGoToSlideID = ((count($arrVod) > 0) ? 1 : 0);
      }
    }
    // 查询该通道下所有有录像的日期列表，注意合并相同日期...
    unset($map); $map['camera_id'] = $camera_id;
    $strField = "DATE_FORMAT(created,'%Y-%m-%d') days";
    $arrMarks = D('record')->where($map)->field($strField)->group('days')->order('days ASC')->select();
    // 设置日期标注点...
    if( count($arrMarks) > 0 ) {
      $strMarks = implode('\',\'', array_column($arrMarks, 'days'));
      $strMarks = sprintf("'%s'", $strMarks);
      $this->assign('my_rec_marks', $strMarks);
    }
    // 设置初始日期，跳转滑块编号...
    $this->assign('my_init_date', $strCurDate);
    $this->assign('my_goto', $theGoToSlideID);
    $this->assign('my_play', $arrList);
    
    // 相关通道 => 得到每页通道数，总记录数，计算总页数...
    unset($map); $pagePer = 16; //C('PAGE_PER');
    $map['gather_id'] = $gather_id;
    $totalNum = D('camera')->where($map)->count();
    $max_page = intval($totalNum / $pagePer);
    // 判断是否是整数倍的页码...
    $max_page += (($totalNum % $pagePer) ? 1 : 0);
    // 设置最大页数，设置模板参数...
    $map['status'] = array('gt', 0);
    $onlineNum = D('camera')->where($map)->count();
    $this->assign('my_gather_id', $gather_id);
    $this->assign('my_online_num', $onlineNum);
    $this->assign('my_total_num', $totalNum);
    $this->assign('max_page', $max_page);
   
    // 设置其它模板参数 => web_tracker_addr 已经自带了协议头 http://或https://
    $this->assign('my_web_tracker', $theWebTracker);
    $this->assign('my_title', $this->m_webTitle . ' - 播放');
    $this->assign('my_nav', $my_nav);
    $this->display('play');
  }
  /**
  +----------------------------------------------------------
  * 显示页面 => vod | live | width | height
  +----------------------------------------------------------
  */
  public function show()
  {
    // 统一设置设置是否是IE浏览器类型...
    $this->assign('my_isIE', $this->m_isIEBrowser);
    // 根据type类型获取url地址...
    if( strcasecmp($_GET['type'], "vod") == 0 ) {
      // 获取点播记录信息 => web_tracker_addr 已经自带了协议头 http://或https://
      $map['record_id'] = $_GET['record_id'];
      $dbVod = D('record')->where($map)->field('record_id,file_fdfs,clicks')->find();
      $dbSys = D('system')->field('web_tracker_addr,web_tracker_port')->find();
      // 设置播放页面需要的数据内容 => 这里为source做特殊处理...
      $arrSource[0]['src'] = sprintf("%s:%d/%s", $dbSys['web_tracker_addr'], $dbSys['web_tracker_port'], $dbVod['file_fdfs']);
      $arrSource[0]['type'] = "video/mp4";
      $dbShow['source'] = json_encode($arrSource);
      // 为了与直播兼容设置的数据...
      $dbShow['player_id'] = -1;
      $dbShow['player_vod'] = 1;
      $dbShow['player_camera'] = -1;
      // 点播以html5优先，flash垫后...
      $arrTech[0] = "html5";
      $arrTech[1] = "flash";
      $dbShow['tech'] = json_encode($arrTech);
      // 反馈点击次数给显示层...
      $dbShow['clicks'] = intval($dbVod['clicks']) + 1;
      $dbShow['click_id'] = "vod_" . $dbVod['record_id'];
      // 累加点播计数器，写入数据库...
      $dbSave['clicks'] = $dbShow['clicks'];
      $dbSave['record_id'] = $dbVod['record_id'];
      D('record')->save($dbSave);
    } else if( strcasecmp($_GET['type'], "live") == 0 ) {
      // 首先，判断通道是否处于直播状态...
      $map['camera_id'] = $_GET['camera_id'];
      $dbCamera = D('LiveView')->where($map)->field('camera_id,clicks,status,gather_id,mac_addr')->find();
      if( $dbCamera['status'] <= 0 ) {
        $this->dispError('当前通道处于离线状态，无法播放！', '请联系管理员，开启通道。');
        return;
      }
      // 中转服务器需要的参数...
      $dbParam['mac_addr'] = $dbCamera['mac_addr'];
      $dbParam['rtmp_live'] = $dbCamera['camera_id'];
      // 获取直播链接地址...
      $dbResult = $this->getRtmpUrlFromTransmit($dbParam);
      // 如果获取连接中转服务器失败...
      if( $dbResult['err_code'] > 0 ) {
        $this->dispError($dbResult['err_msg'], '请联系管理员，汇报错误信息。');
        return;
      }
      // 连接中转服务器成功 => 设置flvjs地址、rtmp地址、hls地址，播放器编号...
      $arrSource[0]['src'] = $dbResult['flv_url'];
      $arrSource[0]['type'] = $dbResult['flv_type'];
      $arrSource[1]['src'] = $dbResult['rtmp_url'];
      $arrSource[1]['type'] = $dbResult['rtmp_type'];
      $arrSource[2]['src'] = $dbResult['hls_url'];
      $arrSource[2]['type'] = $dbResult['hls_type'];
      $dbShow['source'] = json_encode($arrSource);
      // 这3个参数是直播播放器汇报时需要的数据...
      $dbShow['player_camera'] = $dbCamera['camera_id'];
      $dbShow['player_id'] = $dbResult['player_id'];
      $dbShow['player_vod'] = 0;
      // 直播flvjs(替代flash)，flash以优先(延时小)，html5垫后(延时大)...
      $arrTech[0] = "flvjs";
      $arrTech[1] = "flash";
      $arrTech[2] = "html5";
      $dbShow['tech'] = json_encode($arrTech);
      // 反馈点击次数给显示层...
      $dbShow['clicks'] = intval($dbCamera['clicks']) + 1;
      $dbShow['click_id'] = "live_" . $dbCamera['camera_id'];
      // 累加点播计数器，写入数据库...
      $dbCamera['clicks'] = $dbShow['clicks'];
      D('camera')->save($dbCamera);
    }
    // 获取传递过来的视频窗口大小...
    $dbShow['width'] = isset($_GET['width']) ? ($_GET['width'] - 2) : 840;
    $dbShow['height'] = isset($_GET['height']) ? ($_GET['height'] - 3) : 545;
    //print_r($dbShow); exit;
    // 赋值给播放器模板页面...
    $this->assign('my_show', $dbShow);
    $this->display('show');
  }
  //
  // 显示超时模板信息...
  private function dispError($inMsgTitle, $inMsgDesc)
  {
    $this->assign('my_title', '糟糕，出错了');
    $this->assign('my_msg_title', $inMsgTitle);
    $this->assign('my_msg_desc', $inMsgDesc);
    $this->display('Common:error_page');
  }
  //
  // 2017.06.15 - by jackey => 修改了中转服务器代码，转发命令给采集端之后，立即返回rtmp地址，无需等待采集端上传结果，避免阻塞，让播放器自己去等待...
  // 从中转服务器获取直播地址...
  // 成功 => array()
  // 失败 => false
  private function getRtmpUrlFromTransmit(&$dbParam)
  {
    // 尝试链接中转服务器...
    $dbSys = D('system')->field('transmit_addr,transmit_port')->find();
    
    /*// 获取直播频道所在的URL地址...
    $saveJson = json_encode($dbParam);
    $json_data = php_transmit_command($dbSys['transmit_addr'], $dbSys['transmit_port'], kClientPlay, kCmd_Play_Login, $saveJson);
    // 获取的JSON数据有效，转成数组，并判断有没有错误码...
    $arrData = json_decode($json_data, true);
    return $arrData;
    if( !is_array($arrData) || ($arrrData['err_code'] > 0) )
      return;
    // 判断获取到的URL是否有效...
    if( !isset($arrData['rtmp_url']) )
      return;
    // 返回最终获取到的URL地址...
    return $arrData['rtmp_url'];*/
    
    // 通过php扩展插件连接中转服务器 => 性能高...
    $transmit = transmit_connect_server($dbSys['transmit_addr'], $dbSys['transmit_port']);
    // 链接中转服务器失败，直接返回...
    if( !$transmit ) {
      $arrData['err_code'] = true;
      $arrData['err_msg'] = '无法连接中转服务器。';
      return $arrData;
    }
    // 获取直播频道所在的URL地址...
    $saveJson = json_encode($dbParam);
    $json_data = transmit_command(kClientPlay, kCmd_Play_Login, $transmit, $saveJson);
    // 关闭中转服务器链接...
    transmit_disconnect_server($transmit);
    // 获取的JSON数据有效，转成数组，直接返回...
    $arrData = json_decode($json_data, true);
    if( !$arrData ) {
      $arrData['err_code'] = true;
      $arrData['err_msg'] = '从中转服务器获取数据失败。';
      return $arrData;
    }
    // 通过错误码，获得错误信息...
    $arrData['err_msg'] = getTransmitErrMsg($arrData['err_code']);
    // 将整个数组返回...
    return $arrData;
  }
}
?>