<?php
/*************************************************************
    Wan (C)2015 - 2016 happyhope.net
    备注：专门处理测试代码...
*************************************************************/

class LoginAction extends Action
{
  //
  // 初始化，加载网站登录配置...
  public function _initialize()
  {
    $this->m_weLogin = C('WECHAT_LOGIN');
  }
  //
  // 执行微信用户的登录授权操作...
  public function doWechatAuth($strCode, $strState)
  {
    // 准备需要返回的数据...
    $strError = ""; $strUnionid = "";
    $strLocation = ""; $strHeadUrl = "";
    do {
      // 从state当中解析回调函数 => 需要进一步的深入解析...
      $strBackUrl = urlsafe_b64decode($strState);
      $pieces = explode('/node_tag/', $strBackUrl);
      // 判断节点标记不能为空...
      if( count($pieces) != 2 || strlen($pieces[0]) <= 0 || strlen($pieces[1]) <= 0 ) {
        $strError = 'error: node_tag is null';
        break;
      }
      // 保存截取之后的数据...
      $strBackUrl = $pieces[0];
      $strNodeTag = $pieces[1];
      // 根据节点标记获取或创建一条新记录...
      $map['node_tag'] = $strNodeTag;
      $dbNode = D('node')->where($map)->find();
      if( count($dbNode) <= 0 ) {
        // 创建一条新纪录...
        $dbNode['node_name'] = "新建节点";
        $dbNode['node_tag'] = $strNodeTag;
        $dbNode['created'] = date('Y-m-d H:i:s');
        $dbNode['updated'] = date('Y-m-d H:i:s');
        $dbNode['node_id'] = D('node')->add($dbNode);
      }
      // 判断获取的节点记录是否有效...
      if( $dbNode['node_id'] <= 0 ) {
        $strError = 'error: node_id is null';
        break;
      }
      // 通过code获取access_token...
      $result = http_get('https://api.weixin.qq.com/sns/oauth2/access_token?appid='.$this->m_weLogin['appid'].'&secret='.$this->m_weLogin['appsecret'].'&code='.$strCode.'&grant_type=authorization_code');
      if( !$result ) {
        $strError = 'error: get_access_token';
        break;
      }
      // 将返回信息转换成数组...
      $arrToken = json_decode($result, true);
      if( isset($arrToken['errcode']) ) {
        $strError = 'error: errorcode='.$arrToken['errcode'].',errormsg='.$arrToken['errmsg'];
        break;
      }
      // 通过access_token获取用户信息...
      $result = http_get('https://api.weixin.qq.com/sns/userinfo?access_token='.$arrToken['access_token'].'&openid='.$arrToken['openid']);
      if( !result ) {
        $strError = 'error: get_user_info';
        break;
      }
      // 将返回信息转换成数组...
      $arrUser = json_decode($result, true);
      if( isset($arrUser['errcode']) ) {
        $strError = 'error: errorcode='.$arrUser['errcode'].',errormsg='.$arrUser['errmsg'];
        break;
      }
      // 微信昵称中去除emoji表情符号的操作...
      $arrUser['nickname'] = trimEmo($arrUser['nickname']);
      // 将获取到的用户关键帧查找数据库内容...
      $dbUser = D('user')->where('wx_unionid="'.$arrUser['unionid'].'"')->find();
      
      // 给获取到的用户设置对应的网站节点编号...
      $dbUser['node_id'] = $dbNode['node_id'];
      
      // 从微信获取的信息更新到数据库当中...
      // 这里是网站应用，不能得到是否关注公众号...
      $dbUser['wx_unionid'] = $arrUser['unionid'];    // 全局唯一ID
      $dbUser['wx_openid_web'] = $arrUser['openid'];  // 本应用的openid
      $dbUser['wx_nickname'] = $arrUser['nickname'];  // 微信昵称
      $dbUser['wx_language'] = $arrUser['language'];  // 语言
      $dbUser['wx_headurl'] = $arrUser['headimgurl']; // 0,46,64,96,132
      $dbUser['wx_country'] = $arrUser['country'];    // 国家
      $dbUser['wx_province'] = $arrUser['province'];  // 省份
      $dbUser['wx_city'] = $arrUser['city'];          // 城市
      $dbUser['wx_sex'] = $arrUser['sex'];            // 性别
      
      // 根据id字段判断是否有记录...
      if( isset($dbUser['user_id']) ) {
        // 更新已有的用户记录...
        $dbUser['update_time'] = date('Y-m-d H:i:s');
        D('user')->where('user_id='.$dbUser['user_id'])->save($dbUser);
      } else {
        // 新建一条用户记录，保存为普通用户...
        $dbUser['user_tick'] = USER_NORMAL_TICK;
        $dbUser['create_time'] = date('Y-m-d H:i:s');
        $dbUser['update_time'] = date('Y-m-d H:i:s');
        $insertid = D('user')->add($dbUser);
        $dbUser['user_id'] = $insertid;
      }
      // 操作成功，记录用户网站登录行为...
      //$dbAction['user_id'] = $dbUser['user_id'];
      //$dbAction['action_id'] = ACTION_WEB_LOGIN;
      //$dbAction['action_time'] = date('Y-m-d H:i:s');
      //$insertid = D('action')->add($dbAction);
      
      // 保存需要返回的用户唯一编号、用户头像地址、用户标记...
      $strUserTick = $dbUser['user_tick'];
      $strHeadUrl = $arrUser['headimgurl'];
      $strUnionid = $arrUser['unionid'];
    }while( false );
    // 将需要返回的参数进行base64编码处理...
    if( strlen($strError) > 0 ) {
      $strError = urlsafe_b64encode($strError);
      $strLocation = sprintf("location:%s/wx_error/%s", $strBackUrl, $strError);
    } else {
      $strUnionid = urlsafe_b64encode($strUnionid);
      $strHeadUrl = urlsafe_b64encode($strHeadUrl);
      $strUserTick = urlsafe_b64encode($strUserTick);
      $strLocation = sprintf("location:%s/wx_unionid/%s/wx_headurl/%s/wx_ticker/%s", $strBackUrl, $strUnionid, $strHeadUrl, $strUserTick);
    }
    // 跳转页面到第三方回调地址...
    header($strLocation);
  }
  //
  // 通过wx_unionid获取微信用户信息...
  // 返回：json => err_code | err_msg | data
  public function getWxUser()
  {
    // 准备返回的对象...
    $arrErr['err_code'] = false;
    $arrErr['err_msg'] = "OK";
    do {
      // 判断输入参数是否有效...
      if( !isset($_GET['wx_unionid']) ) {
        $arrErr['err_code'] = true;
        $arrErr['err_msg'] = '请检查输入参数!';
        break;
      }
      // 通过 wx_unionid 查询用户信息 => 只返回该用户的微信相关信息...
      $map['wx_unionid'] = $_GET['wx_unionid'];
      $dbUser = D('user')->where($map)->field('wx_nickname,wx_language,wx_headurl,wx_country,wx_province,wx_city,wx_sex,update_time')->find();
      if( !$dbUser ) {
        $arrErr['err_code'] = true;
        $arrErr['err_msg'] = '没有找到用户信息!';
        break;
      }
      // 将用户信息赋给返回变量...
      $arrErr['data'] = $dbUser;
    }while( false );
    // 将数组转换成json数据包...
    echo json_encode($arrErr);
  }
  //
  // 保存修改后的用户记录...
  public function saveUser()
  {
    echo D('user')->save($_POST);
  }
  //
  // 通过用户编号获取完整记录信息...
  // 返回：string(json)
  public function getUser()
  {
    $map['user_id'] = $_GET['user_id'];
    $dbUser = D('user')->where($map)->find();
    echo json_encode($dbUser);
  }
  //
  // 获取用户记录总数 => node_tag 筛选...
  // 返回：number
  public function getUserCount()
  {
    $map['node_tag'] = $_GET['node_tag'];
    echo D('UserView')->where($map)->count();
  }
  //
  // 获取用户分页数据 => node_tag 筛选...
  // 返回：string(json)
  public function getPageUser()
  {
    $map['node_tag'] = $_GET['node_tag'];
    $strLimit = urlsafe_b64decode($_GET['limit']);
    $arrUser = D('UserView')->where($map)->limit($strLimit)->order('user_id DESC')->select();
    echo json_encode($arrUser);
  }
}
?>