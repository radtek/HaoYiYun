<?php
/*************************************************************
    Wan (C)2016 - 2017 myhaoyi.com
    备注：用户登录接口...
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
      $strJson = urlsafe_b64decode($strState);
      $arrJson = json_decode($strJson, true);
      // 判断节点标记不能为空...
      if( !isset($arrJson['node_addr']) || !isset($arrJson['node_url']) || 
          !isset($arrJson['node_tag']) || !isset($arrJson['node_type']) ||
          !isset($arrJson['node_name']) )
      {
        $strError = 'error: node_tag is null';
        break;
      }
      // 保存截取之后的数据 => node_addr 已经带了端口信息...
      $strBackUrl = sprintf("http://%s%s", $arrJson['node_addr'], $arrJson['node_url']);
      $strNodeName = $arrJson['node_name'];
      $strNodeAddr = $arrJson['node_addr'];
      $strNodeType = $arrJson['node_type'];
      $strNodeTag = $arrJson['node_tag'];
      // 根据节点标记获取或创建一条新记录...
      $map['node_tag'] = $strNodeTag;
      $dbNode = D('node')->where($map)->find();
      if( count($dbNode) <= 0 ) {
        // 创建一条新纪录...
        $dbNode['node_name'] = $strNodeName;
        $dbNode['node_addr'] = $strNodeAddr;
        $dbNode['node_type'] = $strNodeType;
        $dbNode['node_tag'] = $strNodeTag;
        $dbNode['created'] = date('Y-m-d H:i:s');
        $dbNode['updated'] = date('Y-m-d H:i:s');
        $dbNode['node_id'] = D('node')->add($dbNode);
      } else {
        // 修改已有的记录...
        $dbNode['node_name'] = $strNodeName;
        $dbNode['node_addr'] = $strNodeAddr;
        $dbNode['node_type'] = $strNodeType;
        $dbNode['node_tag'] = $strNodeTag;
        $dbNode['updated'] = date('Y-m-d H:i:s');
        D('node')->save($dbNode);
      }
      // 判断获取的节点记录是否有效...
      if( $dbNode['node_id'] <= 0 ) {
        $strError = 'error: node_id is null';
        break;
      }
      // 准备请求需要的url地址...
      $strTokenUrl = sprintf("https://api.weixin.qq.com/sns/oauth2/access_token?appid=%s&secret=%s&code=%s&grant_type=authorization_code",
                              $this->m_weLogin['appid'], $this->m_weLogin['appsecret'], $strCode);
      // 通过code获取access_token...
      $result = http_get($strTokenUrl);
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
      // 准备请求需要的url地址...
      $strUserUrl = sprintf("https://api.weixin.qq.com/sns/userinfo?access_token=%s&openid=%s&lang=zh_CN", $arrToken['access_token'], $arrToken['openid']);
      // 通过access_token获取用户信息...
      $result = http_get($strUserUrl);
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
      // 将获取到的用户关键帧查找数据库内容 => 这里不能用$map做为数组变量，否则，查询失败...
      $where['wx_unionid'] = $arrUser['unionid'];
      $dbUser = D('user')->where($where)->find();
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
        $condition['user_id'] = $dbUser['user_id'];
        D('user')->where($condition)->save($dbUser);
      } else {
        // 新建一条用户记录...
        $dbUser['create_time'] = date('Y-m-d H:i:s');
        $dbUser['update_time'] = date('Y-m-d H:i:s');
        $insertid = D('user')->add($dbUser);
        $dbUser['user_id'] = $insertid;
      }
      // 保存需要返回的用户信息 => 全部转换成JSON...
      $strWxJSON = json_encode($arrUser);
    }while( false );
    // 将需要返回的参数进行base64编码处理...
    if( strlen($strError) > 0 ) {
      $strError = urlsafe_b64encode($strError);
      $strLocation = sprintf("Location: %s/wx_error/%s", $strBackUrl, $strError);
    } else {
      $strWxJSON = urlsafe_b64encode($strWxJSON);
      $strLocation = sprintf("Location: %s/wx_json/%s", $strBackUrl, $strWxJSON);
    }
    // 跳转页面到第三方回调地址...
    header($strLocation);
  }
}
?>