<?php
/*************************************************************
    HaoYi (C)2017 - 2018 myhaoyi.com
    备注：专门处理第三方接口调用集合...
*************************************************************/

class ErrCode {
	public static $GM_OK            = 0;               // OK
  public static $GM_BadUnionid    = 1001;            // 无效的unionid
  public static $GM_NoUser        = 1002;            // 没有找到unionid指定的帐号
  public static $GM_NoAuth        = 1003;            // unionid对应的帐号没有接口访问权限
  public static $GM_NoToken       = 1004;            // 没有凭证，请确认参数
  public static $GM_BadToken      = 1005;            // 生成token过程中发生错误
  public static $GM_ParseToken    = 1006;            // 解析token失败
  public static $GM_ExpireToken   = 1007;            // 凭证已过期
  public static $GM_NoName        = 1008;            // 没有名称，请确认参数
  public static $GM_NoAddr        = 1009;            // 没有地址，请确认参数
  public static $GM_NoID          = 1010;            // 没有编号，请确认参数
  public static $GM_BadTime       = 1011;            // 时间格式不正确
  public static $GM_OverTime      = 1012;            // 时间发生重叠
  public static function getErrMsg($inErrCode) {
    $strErrMsg = "OK";
    switch( $inErrCode ) {
      case $GM_OK:  $strErrMsg = "OK"; break;
      case $GM_BadUnionid: $strErrMsg = "无效的unionid"; break;
      case $GM_NoUser: $strErrMsg = "没有找到unionid指定的帐号"; break;
      case $GM_NoAuth: $strErrMsg = "unionid对应的帐号没有接口访问权限"; break;
      case $GM_NoToken: $strErrMsg = "没有凭证，请确认参数"; break;
      case $GM_BadToken: $strErrMsg = "生成token过程中发生错误"; break;
      case $GM_ParseToken: $strErrMsg = "解析token失败"; break;
      case $GM_ExpireToken: $strErrMsg = "凭证已过期，请重新获取"; break;
      case $GM_NoName: $strErrMsg = "没有名称，请确认参数"; break;
      case $GM_NoAddr: $strErrMsg = "没有地址，请确认参数"; break;
      case $GM_NoID: $strErrMsg = "没有编号，请确认参数"; break;
      case $GM_BadTime: $strErrMsg = "时间格式不正确"; break;
      default: $strErrMsg = "未定义的错误号"; break;
    }
    return $strErrMsg;
  }
}

class APIAction extends Action
{
  // 定义加密算法、凭证有效器、凭证加密因子...
  private $m_token_expires = 7200;
  private $m_token_cipher = "DES-ECB";
  private $m_token_key = "954CF22C-03A6-40B1-BB04-A1EB7F9783A2";
  private $m_data = array('err_code' => 0, 'err_msg' => "OK");
  //
  // 定义默认的返回信息头，设置默认的错误号...
  public function _initialize() {
  }
  //
  // 统一设置错误接口函数...
  private function setError($inErrCode) {
    $this->m_data['err_code'] = $inErrCode;
    $this->m_data['err_msg'] = ErrCode::getErrMsg($inErrCode);    
  }
  //
  // 统一解析token接口函数...
  private function parseToken($token) {
    // 没有token凭证，返回错误...
    if( !isset($_GET['token']) ) {
      $this->setError(ErrCode::$GM_NoToken);
      return false;
    }
    // 解析凭证，获取unionid和expires...
    $arrToken = $this->decrypt($_GET['token']);
    // 解析凭证失败，或解析到的数据有误，直接返回错误...
    if( !$arrToken || !isset($arrToken['unionid']) || !isset($arrToken['expires']) ) {
      $this->setError(ErrCode::$GM_ParseToken);
      return false;
    }
    // 检测凭证是否已经过期，如果过期，返回错误号...
    if( time(NULL) >= $arrToken['expires'] ) {
      $this->setError(ErrCode::$GM_ExpireToken);
      return false;
    }
    // 解析正确，返回数组...
    return $arrToken;
  }
  //
  // 对unionid进行加密，返回token...
  private function encrypt($unionid)
  {
    $unionid = serialize($unionid);
    $data['expires'] = base64_encode(time(NULL) + $this->m_token_expires);
    $data['token'] = openssl_encrypt($unionid, $this->m_token_cipher, $this->m_token_key, 0, $data['expires']);
    if( !$data['token'] ) return false;
    $token = base64_encode(json_encode($data));
    return $token;
  }
  //
  // 对token进行解密，得到unionid和expires...
  private function decrypt($token)
  {
    $encrypt = json_decode(base64_decode($token), true);
    $expires = base64_decode($encrypt['expires']);
    $unionid = openssl_decrypt($encrypt['token'], $this->m_token_cipher, $this->m_token_key, 0, $expires);
    if( !$unionid ) return false;
    $unionid = unserialize($unionid);
    $data['unionid'] = $unionid;
    $data['expires'] = $expires;
    return $data;
  }
  //
  // 获取访问凭证...
  public function access_token() {
    do {
      // 判断输入参数是否有效...
      if( !isset($_GET['unionid']) ) {
        $this->setError(ErrCode::$GM_BadUnionid);
        break;
      }
      // 对数据进行base64还原...
      $unionid = base64_decode($_GET['unionid']);
      // 在数据库中寻找指定的用户...
      $condition['wx_unionid'] = $unionid;
      $dbUser = D('user')->where($condition)->find();
      // 没有找到指定的用户，返回错误信息...
      if( !$dbUser ) {
        $this->setError(ErrCode::$GM_NoUser);
        break;
      }
      // 查看是否有权限调用API接口...
      if( $dbUser['user_tick'] != USER_ADMIN_TICK ) {
        $this->setError(ErrCode::$GM_NoAuth);
        break;
      }
      // 找到指定用户，生成token...
      $token = $this->encrypt($unionid);
      // 生成token失败，返回错误信息...
      if( !$token ) {
        $this->setError(ErrCode::$GM_BadToken);
        break;
      }
      // 生成token成功，保存并设置过期时间...
      $this->m_data['token'] = $token;
      $this->m_data['expires'] = $this->m_token_expires;
    }while( false );
    // 返回json数据包...
    echo json_encode($this->m_data);
  }
  //
  // 获取接口版本信息...
  public function version() {
    $dbSys = D('system')->field('web_type,web_title')->find();
    $arrVer['API'] = "v1";
    $arrVer['Version'] = C('VERSION');
    $arrVer['Name'] = $dbSys['web_title'];
    $arrVer['Server'] = sprintf("%s %s", php_uname('s'), php_uname('r'));
    echo json_encode($arrVer);
  }
  //
  // 获取学校记录列表...
  public function list_school() {
    do {
      // 调用统一解析函数，解析token...
      $arrToken = $this->parseToken();
      // 解析失败，直接中断，底层已经填充错误信息...
      if( !$arrToken ) break;
      // 准备需要的分页参数...
      $pagePer = (isset($_GET['per']) ? $_GET['per'] : C('PAGE_PER')); // 每页显示的条数...
      $pageCur = (isset($_GET['cur']) ? $_GET['cur'] : 1);  // 当前页码...
      $pageLimit = (($pageCur-1)*$pagePer).','.$pagePer; // 读取范围...
      // 获取记录总数和总页数...
      $totalNum = D('school')->count();
      $max_page = intval($totalNum / $pagePer);
      // 判断是否是整数倍的页码...
      $max_page += (($totalNum % $pagePer) ? 1 : 0);
      // 查询分页数据...
      $arrSchool = D('school')->limit($pageLimit)->order('school_id DESC')->select();
      // 填充需要返回的信息...
      $this->m_data['total_num'] = $totalNum;
      $this->m_data['max_page'] = $max_page;
      $this->m_data['cur_page'] = $pageCur;
      $this->m_data['per_page'] = $pagePer;
      $this->m_data['data'] = $arrSchool;
    }while( false );
    // 返回json数据包...
    echo json_encode($this->m_data);
  }
  //
  // 添加学校...
  public function add_school() {
    do {
      $arrToken = $this->parseToken();
      if( !$arrToken ) break;
      // 没有名称...
      if( !isset($_GET['name']) ) {
        $this->setError(ErrCode::$GM_NoName);
        break;
      }
      // 没有地址...
      if( !isset($_GET['addr']) ) {
        $this->setError(ErrCode::$GM_NoAddr);
        break;
      }
      // 构造添加记录需要的数据...
      $arrData = $_GET;
      $arrData['created'] = date('Y-m-d H:i:s');
      $arrData['updated'] = date('Y-m-d H:i:s');
      $this->m_data['school_id'] = D('school')->add($arrData);
    }while( false );
    // 返回json数据包...
    echo json_encode($this->m_data);
  }
  //
  // 修改学校...
  public function mod_school() {
    do {
      $arrToken = $this->parseToken();
      if( !$arrToken ) break;
      // 没有编号...
      if( !isset($_GET['school_id']) ) {
        $this->setError(ErrCode::$GM_NoID);
        break;
      }
      // 没有名称...
      if( !isset($_GET['name']) ) {
        $this->setError(ErrCode::$GM_NoName);
        break;
      }
      // 没有地址...
      if( !isset($_GET['addr']) ) {
        $this->setError(ErrCode::$GM_NoAddr);
        break;
      }
      // 更新到数据库当中...
      D('school')->save($_GET);
    }while( false );
    // 返回json数据包...
    echo json_encode($this->m_data);
  }
  // 删除学校...
  public function del_school() {
    do {
      $arrToken = $this->parseToken();
      if( !$arrToken ) break;
      // 没有编号...
      if( !isset($_GET['school_id']) ) {
        $this->setError(ErrCode::$GM_NoID);
        break;
      }
      // 删除记录，批量删除...
      $condition['school_id'] = array('in', $_GET['school_id']);
      D('school')->where($condition)->delete();
    }while( false );
    // 返回json数据包...
    echo json_encode($this->m_data);
  }
  //
  // 获取科目列表...
  public function list_subject() {
    do {
      // 调用统一解析函数，解析token...
      $arrToken = $this->parseToken();
      // 解析失败，直接中断，底层已经填充错误信息...
      if( !$arrToken ) break;
      // 准备需要的分页参数...
      $pagePer = (isset($_GET['per']) ? $_GET['per'] : C('PAGE_PER')); // 每页显示的条数...
      $pageCur = (isset($_GET['cur']) ? $_GET['cur'] : 1);  // 当前页码...
      $pageLimit = (($pageCur-1)*$pagePer).','.$pagePer; // 读取范围...
      // 获取记录总数和总页数...
      $totalNum = D('subject')->count();
      $max_page = intval($totalNum / $pagePer);
      // 判断是否是整数倍的页码...
      $max_page += (($totalNum % $pagePer) ? 1 : 0);
      // 查询分页数据...
      $arrSubject = D('subject')->limit($pageLimit)->order('subject_id DESC')->select();
      // 填充需要返回的信息...
      $this->m_data['total_num'] = $totalNum;
      $this->m_data['max_page'] = $max_page;
      $this->m_data['cur_page'] = $pageCur;
      $this->m_data['per_page'] = $pagePer;
      $this->m_data['data'] = $arrSubject;
    }while( false );
    // 返回json数据包...
    echo json_encode($this->m_data);
  }
  //
  // 添加科目...
  public function add_subject() {
    do {
      $arrToken = $this->parseToken();
      if( !$arrToken ) break;
      // 没有名称...
      if( !isset($_GET['subject_name']) ) {
        $this->setError(ErrCode::$GM_NoName);
        break;
      }
      // 构造添加记录需要的数据...
      $arrData = $_GET;
      $arrData['created'] = date('Y-m-d H:i:s');
      $arrData['updated'] = date('Y-m-d H:i:s');
      $this->m_data['subject_id'] = D('subject')->add($arrData);
    }while( false );
    // 返回json数据包...
    echo json_encode($this->m_data);
  }  
  //
  // 修改科目...
  public function mod_subject() {
    do {
      $arrToken = $this->parseToken();
      if( !$arrToken ) break;
      // 没有编号...
      if( !isset($_GET['subject_id']) ) {
        $this->setError(ErrCode::$GM_NoID);
        break;
      }
      // 没有名称...
      if( !isset($_GET['subject_name']) ) {
        $this->setError(ErrCode::$GM_NoName);
        break;
      }
      // 更新到数据库当中...
      D('subject')->save($_GET);
    }while( false );
    // 返回json数据包...
    echo json_encode($this->m_data);
  }
  //
  // 删除科目...
  public function del_subject() {
    do {
      $arrToken = $this->parseToken();
      if( !$arrToken ) break;
      // 没有编号...
      if( !isset($_GET['subject_id']) ) {
        $this->setError(ErrCode::$GM_NoID);
        break;
      }
      // 删除记录，批量删除...
      $condition['subject_id'] = array('in', $_GET['subject_id']);
      D('subject')->where($condition)->delete();
    }while( false );
    // 返回json数据包...
    echo json_encode($this->m_data);
  }
  //
  // 获取年级列表..
  public function list_grade() {
    do {
      // 调用统一解析函数，解析token...
      $arrToken = $this->parseToken();
      // 解析失败，直接中断，底层已经填充错误信息...
      if( !$arrToken ) break;
      // 准备需要的分页参数...
      $pagePer = (isset($_GET['per']) ? $_GET['per'] : C('PAGE_PER')); // 每页显示的条数...
      $pageCur = (isset($_GET['cur']) ? $_GET['cur'] : 1);  // 当前页码...
      $pageLimit = (($pageCur-1)*$pagePer).','.$pagePer; // 读取范围...
      // 获取记录总数和总页数...
      $totalNum = D('grade')->count();
      $max_page = intval($totalNum / $pagePer);
      // 判断是否是整数倍的页码...
      $max_page += (($totalNum % $pagePer) ? 1 : 0);
      // 查询分页数据...
      $arrGrade = D('grade')->limit($pageLimit)->order('grade_id DESC')->select();
      // 填充需要返回的信息...
      $this->m_data['total_num'] = $totalNum;
      $this->m_data['max_page'] = $max_page;
      $this->m_data['cur_page'] = $pageCur;
      $this->m_data['per_page'] = $pagePer;
      $this->m_data['data'] = $arrGrade;
    }while( false );
    // 返回json数据包...
    echo json_encode($this->m_data);
  }
  //
  // 获取教师列表...
  public function list_teacher() {
    do {
      // 调用统一解析函数，解析token...
      $arrToken = $this->parseToken();
      // 解析失败，直接中断，底层已经填充错误信息...
      if( !$arrToken ) break;
      // 准备需要的分页参数...
      $pagePer = (isset($_GET['per']) ? $_GET['per'] : C('PAGE_PER')); // 每页显示的条数...
      $pageCur = (isset($_GET['cur']) ? $_GET['cur'] : 1);  // 当前页码...
      $pageLimit = (($pageCur-1)*$pagePer).','.$pagePer; // 读取范围...
      // 获取记录总数和总页数...
      $totalNum = D('teacher')->count();
      $max_page = intval($totalNum / $pagePer);
      // 判断是否是整数倍的页码...
      $max_page += (($totalNum % $pagePer) ? 1 : 0);
      // 查询分页数据...
      $arrTeacher = D('teacher')->limit($pageLimit)->order('teacher_id DESC')->select();
      // 填充需要返回的信息...
      $this->m_data['total_num'] = $totalNum;
      $this->m_data['max_page'] = $max_page;
      $this->m_data['cur_page'] = $pageCur;
      $this->m_data['per_page'] = $pagePer;
      $this->m_data['data'] = $arrTeacher;
    }while( false );
    // 返回json数据包...
    echo json_encode($this->m_data);
  }
  //
  // 添加教师...
  public function add_teacher() {
      do {
      $arrToken = $this->parseToken();
      if( !$arrToken ) break;
      // 没有名称...
      if( !isset($_GET['teacher_name']) || !isset($_GET['title_name']) || !isset($_GET['sex_name']) ) {
        $this->setError(ErrCode::$GM_NoName);
        break;
      }
      // 没有职称编号...
      if( !isset($_GET['title_id']) ) {
        $this->setError(ErrCode::$GM_NoID);
        break;
      }
      // 构造添加记录需要的数据...
      $arrData = $_GET;
      $arrData['created'] = date('Y-m-d H:i:s');
      $arrData['updated'] = date('Y-m-d H:i:s');
      $this->m_data['teacher_id'] = D('teacher')->add($arrData);
    }while( false );
    // 返回json数据包...
    echo json_encode($this->m_data);
  }
  //
  // 修改教师...
  public function mod_teacher() {
    do {
      $arrToken = $this->parseToken();
      if( !$arrToken ) break;
      // 没有编号...
      if( !isset($_GET['teacher_id']) ) {
        $this->setError(ErrCode::$GM_NoID);
        break;
      }
      // 没有名称 => 教师名称、性别、职称全部没有才报错...
      if( !isset($_GET['teacher_name']) && !isset($_GET['sex_name']) && !isset($_GET['title_name']) ) {
        $this->setError(ErrCode::$GM_NoName);
        break;
      }
      // 更新到数据库当中...
      D('teacher')->save($_GET);
    }while( false );
    // 返回json数据包...
    echo json_encode($this->m_data);    
  }
  //
  // 删除教师...
  public function del_teacher() {
    do {
      $arrToken = $this->parseToken();
      if( !$arrToken ) break;
      // 没有编号...
      if( !isset($_GET['teacher_id']) ) {
        $this->setError(ErrCode::$GM_NoID);
        break;
      }
      // 删除记录，批量删除...
      $condition['teacher_id'] = array('in', $_GET['teacher_id']);
      D('teacher')->where($condition)->delete();
    }while( false );
    // 返回json数据包...
    echo json_encode($this->m_data);
  }
  //
  // 获取采集端列表...
  public function list_gather() {
    do {
      // 调用统一解析函数，解析token...
      $arrToken = $this->parseToken();
      // 解析失败，直接中断，底层已经填充错误信息...
      if( !$arrToken ) break;
      // 准备需要的分页参数...
      $pagePer = (isset($_GET['per']) ? $_GET['per'] : C('PAGE_PER')); // 每页显示的条数...
      $pageCur = (isset($_GET['cur']) ? $_GET['cur'] : 1);  // 当前页码...
      $pageLimit = (($pageCur-1)*$pagePer).','.$pagePer; // 读取范围...
      // 获取记录总数和总页数...
      $totalNum = D('gather')->count();
      $max_page = intval($totalNum / $pagePer);
      // 判断是否是整数倍的页码...
      $max_page += (($totalNum % $pagePer) ? 1 : 0);
      // 查询分页数据...
      $arrGather = D('gather')->limit($pageLimit)->order('gather_id DESC')->select();
      // 填充需要返回的信息...
      $this->m_data['total_num'] = $totalNum;
      $this->m_data['max_page'] = $max_page;
      $this->m_data['cur_page'] = $pageCur;
      $this->m_data['per_page'] = $pagePer;
      $this->m_data['data'] = $arrGather;
    }while( false );
    // 返回json数据包...
    echo json_encode($this->m_data);
  }
  //
  // 修改采集端...
  public function mod_gather() {
    do {
      $arrToken = $this->parseToken();
      if( !$arrToken ) break;
      // 没有编号...
      if( !isset($_GET['gather_id']) ) {
        $this->setError(ErrCode::$GM_NoID);
        break;
      }
      // 更新到数据库当中...
      D('gather')->save($_GET);
    }while( false );
    // 返回json数据包...
    echo json_encode($this->m_data);    
  }
  //
  // 获取通道列表...
  public function list_camera() {
    do {
      // 调用统一解析函数，解析token...
      $arrToken = $this->parseToken();
      // 解析失败，直接中断，底层已经填充错误信息...
      if( !$arrToken ) break;
      // 准备需要的分页参数...
      $pagePer = (isset($_GET['per']) ? $_GET['per'] : C('PAGE_PER')); // 每页显示的条数...
      $pageCur = (isset($_GET['cur']) ? $_GET['cur'] : 1);  // 当前页码...
      $pageLimit = (($pageCur-1)*$pagePer).','.$pagePer; // 读取范围...
      // 获取记录总数和总页数...
      $totalNum = D('LiveView')->count();
      $max_page = intval($totalNum / $pagePer);
      // 判断是否是整数倍的页码...
      $max_page += (($totalNum % $pagePer) ? 1 : 0);
      // 查询分页数据...
      $arrCamera = D('LiveView')->limit($pageLimit)->order('camera_id DESC')->select();
      // 组合快照截图的存储服务器地址...
      $dbSys = D('system')->field('web_tracker_addr,web_tracker_port')->find();
      $this->m_data['storage_addr'] = sprintf("%s:%d", $dbSys['web_tracker_addr'], $dbSys['web_tracker_port']);
      // 填充需要返回的信息...
      $this->m_data['total_num'] = $totalNum;
      $this->m_data['max_page'] = $max_page;
      $this->m_data['cur_page'] = $pageCur;
      $this->m_data['per_page'] = $pagePer;
      $this->m_data['data'] = $arrCamera;
    }while( false );
    // 返回json数据包...
    echo json_encode($this->m_data);
  }
  //
  // 添加通道...
  public function add_camera() {
      do {
      $arrToken = $this->parseToken();
      if( !$arrToken ) break;
      // 没有编号...
      if( !isset($_GET['gather_id']) || !isset($_GET['grade_id']) ) {
        $this->setError(ErrCode::$GM_NoID);
        break;
      }
      // 没有编号 => 通道模式错误...
      if( !isset($_GET['stream_prop']) || $_GET['stream_prop'] <= 0 ) {
        $this->setError(ErrCode::$GM_NoID);
        break;
      }
      // 没有名称 => 通道名称为空 => MP4文件不能为空 => 流地址不能为空...
      if( (($_GET['stream_prop'] == 1) && !isset($_GET['stream_mp4'])) ||
          (($_GET['stream_prop'] == 2) && !isset($_GET['stream_url'])) ||
          !isset($_GET['camera_name']) )
      {
        $this->setError(ErrCode::$GM_NoName);
        break;
      }
      // 构造添加记录需要的数据...
      $dbCamera = $_GET;
      $dbCamera['created'] = date('Y-m-d H:i:s');
      $dbCamera['updated'] = date('Y-m-d H:i:s');
      // 创建唯一标识符号，保存到数据库...
      $dbCamera['device_sn'] = md5(uniqid(md5(microtime(true)),true));
      $dbCamera['camera_id'] = D('camera')->add($dbCamera);
      // 保存需要返回的新建编号...
      $this->m_data['camera_id'] = $dbCamera['camera_id'];
      // 获取采集端的mac_addr地址...
      $condition['gather_id'] = $dbCamera['gather_id'];
      $dbGather = D('gather')->where($condition)->field('mac_addr')->find();
      $dbCamera['mac_addr'] = $dbGather['mac_addr'];
      // 连接中转服务器...
      $dbSys = D('system')->field('transmit_addr,transmit_port')->find();
      $transmit = transmit_connect_server($dbSys['transmit_addr'], $dbSys['transmit_port']);
      // 连接成功，执行中转命令...
      if( $transmit ) {
        // 将参数转换成json数据包，发起添加命令...
        $saveJson = json_encode($dbCamera);
        $json_data = transmit_command(kClientPHP, kCmd_PHP_Set_Camera_Add, $transmit, $saveJson);
        // 关闭中转服务器链接...
        transmit_disconnect_server($transmit);
        // 将中转服务器执行结果，返回给调用者...
        $this->m_data['transmit'] = $json_data;
      } else {
        // 连接中转服务器失败...
        $this->m_data['transmit'] = '无法连接中转服务器';
      }
    }while( false );
    // 返回json数据包...
    echo json_encode($this->m_data);
  }
  //
  // 修改通道...
  public function mod_camera() {
    do {
      $arrToken = $this->parseToken();
      if( !$arrToken ) break;
      // 没有编号 => 通道编号必填项...
      if( !isset($_GET['camera_id']) ) {
        $this->setError(ErrCode::$GM_NoID);
        break;
      }
      // 没有名称...
      if( !isset($_GET['camera_name']) ) {
        $this->setError(ErrCode::$GM_NoName);
        break;
      }
      // 查找对应的采集端编号...
      $where['camera_id'] = $_GET['camera_id'];
      $dbItem = D('camera')->where($where)->field('gather_id')->find();
      // 没有找到通道，直接返回错误...
      if( !$dbItem ) {
        $this->setError(ErrCode::$GM_NoID);
        break;
      }
      // 更新到数据库当中...
      $dbCamera = $_GET;
      $dbCamera['gather_id'] = $dbItem['gather_id'];
      $dbCamera['updated'] = date('Y-m-d H:i:s');
      D('camera')->save($dbCamera);
      // 获取采集端的mac_addr地址...
      $condition['gather_id'] = $dbCamera['gather_id'];
      $dbGather = D('gather')->where($condition)->field('mac_addr')->find();
      $dbCamera['mac_addr'] = $dbGather['mac_addr'];
      // 连接中转服务器...
      $dbSys = D('system')->field('transmit_addr,transmit_port')->find();
      $transmit = transmit_connect_server($dbSys['transmit_addr'], $dbSys['transmit_port']);
      // 连接成功，执行中转命令...
      if( $transmit ) {
        // 将参数转换成json数据包，发起修改命令...
        $saveJson = json_encode($dbCamera);
        $json_data = transmit_command(kClientPHP, kCmd_PHP_Set_Camera_Mod, $transmit, $saveJson);
        // 关闭中转服务器链接...
        transmit_disconnect_server($transmit);
        // 将中转服务器执行结果，返回给调用者...
        $this->m_data['transmit'] = $json_data;
      } else {
        // 连接中转服务器失败...
        $this->m_data['transmit'] = '无法连接中转服务器';
      }
    }while( false );
    // 返回json数据包...
    echo json_encode($this->m_data);    
  }
  //
  // 删除通道 => 同时删除录像任务、快照截图、录像文件...
  public function del_camera() {
    do {
      $arrToken = $this->parseToken();
      if( !$arrToken ) break;
      // 没有编号...
      if( !isset($_GET['camera_id']) ) {
        $this->setError(ErrCode::$GM_NoID);
        break;
      }
      //////////////////////////////////
      // 下面是针对采集端的删除操作...
      //////////////////////////////////
      // 组合通道查询条件...
      $condition['camera_id'] = array('in', $_GET['camera_id']);
      // 连接中转服务器...
      $dbSys = D('system')->field('transmit_addr,transmit_port')->find();
      $transmit = transmit_connect_server($dbSys['transmit_addr'], $dbSys['transmit_port']);
      // 连接成功，执行中转命令...
      if( $transmit ) {
        // 查询通道列表...
        $arrLive = D('LiveView')->where($condition)->field('camera_id,gather_id,mac_addr')->select();
        // 遍历每一个通道，发起删除指令...
        foreach($arrLive as &$dbItem) {
          $saveJson = json_encode($dbItem);
          $json_data = transmit_command(kClientPHP, kCmd_PHP_Set_Camera_Del, $transmit, $saveJson);
        }
        // 关闭中转服务器链接...
        transmit_disconnect_server($transmit);
      }
      ///////////////////////////////////
      // 下面是针对数据库的删除操作...
      ///////////////////////////////////
      // 1. 删除该通道下对应的录像课程表...
      D('course')->where($condition)->delete();
      // 2. 删除该通道下对应的录像文件、录像截图...
      $arrList = D('RecordView')->where($condition)->field('record_id,camera_id,file_fdfs,image_id,image_fdfs')->select();
      foreach ($arrList as &$dbVod) {
        // 删除图片和视频文件，逐一删除...
        fastdfs_storage_delete_file1($dbVod['file_fdfs']);
        fastdfs_storage_delete_file1($dbVod['image_fdfs']);
        // 删除图片记录和视频记录...
        D('record')->delete($dbVod['record_id']);
        D('image')->delete($dbVod['image_id']);
      }
      // 3.1 找到通道下所有的图片记录...
      $arrImage = D('image')->where($condition)->field('image_id,camera_id,file_fdfs')->select();
      // 3.2 删除通道快照图片的物理存在 => 逐一删除...
      foreach($arrImage as &$dbImage) {
        fastdfs_storage_delete_file1($dbImage['file_fdfs']);
      }
      // 3.3 删除通道快照图片记录的数据库存在 => 一次性删除...
      D('image')->where($condition)->delete();
      // 4. 直接删除通道列表数据...
      D('camera')->where($condition)->delete();
    }while( false );
    // 返回json数据包...
    echo json_encode($this->m_data);
  }
  //
  // 获取通道状态...
  public function get_camera_status() {
    do {
      $arrToken = $this->parseToken();
      if( !$arrToken ) break;
      // 没有编号...
      if( !isset($_GET['camera_id']) ) {
        $this->setError(ErrCode::$GM_NoID);
        break;
      }
      // 获取通道状态和可能的错误信息...
      $condition['camera_id'] = $_GET['camera_id'];
      $dbCamera = D('camera')->where($condition)->field('camera_id,status,err_code,err_msg')->find();
      // 如果调用失败，返回错误...
      if( !$dbCamera ) {
        $this->setError(ErrCode::$GM_NoID);
        break;
      }
      // 调用成功，直接赋值到对象...
      $this->m_data = $dbCamera;
    }while( false );
    // 返回json数据包...
    echo json_encode($this->m_data);
  }
  //
  // 启动或停止通道统一接口函数...
  private function LunchCamera() {
    do {
      $arrToken = $this->parseToken();
      if( !$arrToken ) break;
      // 没有编号...
      if( !isset($_GET['camera_id']) ) {
        $this->setError(ErrCode::$GM_NoID);
        break;
      }
      // 获取通道状态和可能的错误信息...
      $condition['camera_id'] = $_GET['camera_id'];
      $dbLive = D('LiveView')->where($condition)->field('camera_id,status,mac_addr')->find();
      // 如果调用失败，返回错误...
      if( !$dbLive ) {
        $this->setError(ErrCode::$GM_NoID);
        break;
      }
      // 获取中转服务器地址和端口...
      $dbSys = D('system')->field('transmit_addr,transmit_port')->find();
      // 准备命令需要的数据 => 当前是停止状态，则发起启动；是启动状态，则发起停止...
      $theCmd = (($dbLive['status'] > 0) ? kCmd_PHP_Stop_Camera : kCmd_PHP_Start_Camera );
      // 开始连接中转服务器...
      $transmit = transmit_connect_server($dbSys['transmit_addr'], $dbSys['transmit_port']);
      if( $transmit ) {
        // 构造中转服务器需要的参数...
        $dbParam['camera_id'] = $_GET['camera_id'];
        $dbParam['mac_addr'] = $dbLive['mac_addr'];
        $saveJson = json_encode($dbParam);
        $json_data = transmit_command(kClientPHP, $theCmd, $transmit, $saveJson);
        transmit_disconnect_server($transmit);        
        // 将中转服务器执行结果，返回给调用者...
        $this->m_data['transmit'] = $json_data;
      } else {
        // 连接中转服务器失败...
        $this->m_data['transmit'] = '无法连接中转服务器';
      }
    }while( false );
    // 返回json数据包...
    echo json_encode($this->m_data);
  }
  //
  // 启动通道...
  public function start_camera() {
    $this->LunchCamera();
  }
  //
  // 停止通道...
  public function stop_camera() {
    $this->LunchCamera();
  }
  //
  // 获取录像列表记录...
  public function list_record() {
    do {
      // 调用统一解析函数，解析token...
      $arrToken = $this->parseToken();
      // 解析失败，直接中断，底层已经填充错误信息...
      if( !$arrToken ) break;
      // 准备需要的分页参数...
      $pagePer = (isset($_GET['per']) ? $_GET['per'] : C('PAGE_PER')); // 每页显示的条数...
      $pageCur = (isset($_GET['cur']) ? $_GET['cur'] : 1);  // 当前页码...
      $pageLimit = (($pageCur-1)*$pagePer).','.$pagePer; // 读取范围...
      // 获取记录总数和总页数...
      $totalNum = D('RecordView')->count();
      $max_page = intval($totalNum / $pagePer);
      // 判断是否是整数倍的页码...
      $max_page += (($totalNum % $pagePer) ? 1 : 0);
      // 查询分页数据...
      $arrRecord = D('RecordView')->limit($pageLimit)->order('record_id DESC')->select();
      // 组合快照截图的存储服务器地址...
      $dbSys = D('system')->field('web_tracker_addr,web_tracker_port')->find();
      $this->m_data['storage_addr'] = sprintf("%s:%d", $dbSys['web_tracker_addr'], $dbSys['web_tracker_port']);
      // 填充需要返回的信息...
      $this->m_data['total_num'] = $totalNum;
      $this->m_data['max_page'] = $max_page;
      $this->m_data['cur_page'] = $pageCur;
      $this->m_data['per_page'] = $pagePer;
      $this->m_data['data'] = $arrRecord;
    }while( false );
    // 返回json数据包...
    echo json_encode($this->m_data);
  }
  //
  // 删除录像...
  public function del_record() {
    do {
      $arrToken = $this->parseToken();
      if( !$arrToken ) break;
      // 没有编号...
      if( !isset($_GET['record_id']) ) {
        $this->setError(ErrCode::$GM_NoID);
        break;
      }
      // 删除记录，批量删除...
      $condition['record_id'] = array('in', $_GET['record_id']);
      D('record')->where($condition)->delete();
    }while( false );
    // 返回json数据包...
    echo json_encode($this->m_data);
  }
  //
  // 录像任务列表...
  public function list_course() {
    do {
      $arrToken = $this->parseToken();
      if( !$arrToken ) break;
      // 没有编号...
      if( !isset($_GET['camera_id']) || strlen($_GET['camera_id']) <= 0 ) {
        $this->setError(ErrCode::$GM_NoID);
        break;
      }
      // 准备需要的分页参数...
      $pagePer = (isset($_GET['per']) ? $_GET['per'] : C('PAGE_PER')); // 每页显示的条数...
      $pageCur = (isset($_GET['cur']) ? $_GET['cur'] : 1);  // 当前页码...
      $pageLimit = (($pageCur-1)*$pagePer).','.$pagePer; // 读取范围...
      // 获取记录总数和总页数...
      $totalNum = D('course')->count();
      $max_page = intval($totalNum / $pagePer);
      // 判断是否是整数倍的页码...
      $max_page += (($totalNum % $pagePer) ? 1 : 0);
      // 查询分页数据...
      $condition['camera_id'] = $_GET['camera_id'];
      $arrCourse = D('course')->where($condition)->limit($pageLimit)->select();
      // 填充需要返回的信息...
      $this->m_data['total_num'] = $totalNum;
      $this->m_data['max_page'] = $max_page;
      $this->m_data['cur_page'] = $pageCur;
      $this->m_data['per_page'] = $pagePer;
      $this->m_data['data'] = $arrCourse;
    }while( false );
    // 返回json数据包...
    echo json_encode($this->m_data);
  }
  //////////////////////////////////////////////////////////////
  // 2017.12.21 - 录像任务的接口暂时不写，直接通过后台配置...
  // 创建录像任务...
  //////////////////////////////////////////////////////////////
  /*public function add_course() {
    do {
      $arrToken = $this->parseToken();
      if( !$arrToken ) break;
      // 没有编号...
      if( !isset($_GET['camera_id']) || strlen($_GET['camera_id']) <= 0 ||
          !isset($_GET['subject_id']) || strlen($_GET['subject_id']) <= 0 ||
          !isset($_GET['teacher_id']) || strlen($_GET['teacher_id']) <= 0 ||
          !isset($_GET['week_id']) || strlen($_GET['week_id']) <= 0 ||
          !isset($_GET['elapse_sec']) || strlen($_GET['elapse_sec']) <= 0 || 
          !isset($_GET['start_time']) || strlen($_GET['start_time']) <= 0 )
      {
        $this->setError(ErrCode::$GM_NoID);
        break;
      }
      // 判断日期格式是否正确...
      $nElapseSec = intval($_GET['elaspe_sec']);
      $theStartTime = sprintf("%s %s", date('Y-m-d'), $_GET['start_time']);
      $nStartTime = strtotime($theStartTime);
      // 计算出来的持续时间、开始时间不正确...
      if( $nElapseSec <= 0 || $nStartTime <= 0 ) {
        $this->setError(ErrCode::$GM_BadTime);
        break;
      }
      // 就算出来的结束时间不正确...
      $nEndTime = $nStartTime + $nElapseSec;
      if( $nEndTime <= 0 || $nEndTime <= $nStartTime ) {
        $this->setError(ErrCode::$GM_BadTime);
        break;
      }
      // 计算结束时间字符串格式...
      $theEndTime = strftime ("Y-m-d H:i:s", $nEndTime);
      // 判断输入时间是否与已有时间放生重叠...
      if( !$this->IsTimeOverlaped() ) {
        $this->setError(ErrCode::$GM_OverTime);
        break;
      }
      // 将数据存放到数据库当中...
      $dbCourse = $_GET;
      $dbCourse['start_time'] = $theStartTime;
      $dbCourse['end_time'] = $theEndTime;
    }while( false );
    // 返回json数据包...
    echo json_encode($this->m_data);
  }*/
  //
  // 播放直播通道 => 返回播放地址，可以自定义页面大小，flvjs/flash/h5 自动匹配
  public function play_camera() {
    do {
      $arrToken = $this->parseToken();
      if( !$arrToken ) break;
      // 没有编号...
      if( !isset($_GET['camera_id']) || strlen($_GET['camera_id']) <= 0 ) {
        $this->setError(ErrCode::$GM_NoID);
        break;
      }
      // 得到通道编号...
      $camera_id = $_GET['camera_id'];
      // 没有设置宽度，默认850px，没有设置高度，默认550px
      $theWidth = (isset($_GET['width']) ? $_GET['width'] : 850);
      $theHeight = (isset($_GET['height']) ? $_GET['height'] : 550);
      // 组合需要返回的播放地址内容...
      $strUrl = sprintf("%s://%s/wxapi.php/Home/show/type/live/camera_id/%d/width/%d/height/%d", 
                        $_SERVER['REQUEST_SCHEME'], $_SERVER['HTTP_HOST'],
                        $camera_id, $theWidth, $theHeight); 
      // 将获取到的播放地址，设置到返回对象当中...
      $this->m_data['play_url'] = $strUrl;
    }while( false );
    // 返回json数据包...
    echo json_encode($this->m_data);
  }
  //
  // 播放录像文件 => 返回播放地址，可以自定义页面大小，flash/h5 自动匹配
  public function play_record() {
    do {
      $arrToken = $this->parseToken();
      if( !$arrToken ) break;
      // 没有编号...
      if( !isset($_GET['record_id']) || strlen($_GET['record_id']) <= 0 ) {
        $this->setError(ErrCode::$GM_NoID);
        break;
      }
      // 得到录像文件编号...
      $record_id = $_GET['record_id'];
      // 没有设置宽度，默认850px，没有设置高度，默认550px
      $theWidth = (isset($_GET['width']) ? $_GET['width'] : 850);
      $theHeight = (isset($_GET['height']) ? $_GET['height'] : 550);
      // 组合需要返回的播放地址内容...
      $strUrl = sprintf("%s://%s/wxapi.php/Home/show/type/vod/record_id/%d/width/%d/height/%d", 
                        $_SERVER['REQUEST_SCHEME'], $_SERVER['HTTP_HOST'],
                        $record_id, $theWidth, $theHeight); 
      // 将获取到的播放地址，设置到返回对象当中...
      $this->m_data['play_url'] = $strUrl;
    }while( false );
    // 返回json数据包...
    echo json_encode($this->m_data);
  }
}
?>