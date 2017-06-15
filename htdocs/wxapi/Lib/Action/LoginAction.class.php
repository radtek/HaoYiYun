<?php
/*************************************************************
    HaoYi (C)2017 - 2020 myhaoyi.com

*************************************************************/

class LoginAction extends Action
{
  // 初始化页面的默认操作...
  public function _initialize()
  {
    $this->m_weLogin = C('WECHAT_LOGIN');
  }
  //
  // 显示微信登录的二维码...
  // 参数：wx_error | wx_unionid | wx_headurl
  public function index()
  {
    // 如果有登录错误信息，跳转到错误处理...
    if( isset($_GET['wx_error']) ) {
      $strErr = urlsafe_b64decode($_GET['wx_error']);
      // 这里需要专门编写一个特殊的报错页面...
      $this->dispError($strErr, '糟糕，登录失败了');
      return;
    }
    // 如果找到了用户编号和用户头像，将这个编号解析出来，放入cookie当中...
    if( isset($_GET['wx_unionid']) && isset($_GET['wx_headurl']) && isset($_GET['wx_ticker']) ) {
			// 2017.06.15 - by jackey => 去掉了时间戳验证超时的功能...
      /*// 验证时间戳是否在有效范围之内 => 超过30秒，弹出错误框...
      isset($_GET['code']) && 
			$theStamp = urlsafe_b64decode($_GET['code']);
			if( (time() - $theStamp) > 30 ) {
				$this->dispError('登录超时', '糟糕，登录失败了');
				return;
			}*/
			// 获取服务器反馈的信息...
      $strUnionid = urlsafe_b64decode($_GET['wx_unionid']);
      $strHeadUrl = urlsafe_b64decode($_GET['wx_headurl']);
      $strTicker = urlsafe_b64decode($_GET['wx_ticker']);
      // 把unionid存入cookie当中，有效期设置一整天，必须用Cookie类...
      Cookie::set('wx_unionid', $strUnionid, 3600*24);
      Cookie::set('wx_headurl', $strHeadUrl, 3600*24);
      Cookie::set('wx_ticker', $strTicker, 3600*24);
      // 用户登录成功，进行页面跳转...
      A('Admin')->index();
      return;
    }
    // 如果当前的cookie是有效的，直接进行页面跳转...
    if( Cookie::is_set('wx_unionid') && Cookie::is_set('wx_headurl') && Cookie::is_set('wx_ticker') ) {
      A('Admin')->index();
      return;
    }
    // cookie不是完全有效，重置cookie...
    setcookie('wx_unionid','',-1,'/');
    setcookie('wx_headurl','',-1,'/');
    setcookie('wx_ticker','',-1,'/');

    ///////////////////////////////////////////////////////////
    // 没有cookie，没有错误，没有unionid，说明是正常登录...
    ///////////////////////////////////////////////////////////

		// 2017.06.15 - by jackey => 去掉了时间戳验证超时的功能...
    //$state = sprintf("http://%s%s/Login/index/code/%s", $_SERVER['HTTP_HOST'], __APP__, urlsafe_b64encode(time()));
    
    // 拼接当前访问页面的完整链接地址（增加时间戳标记） => 登录服务器会反向调用...
    $state = sprintf("http://%s%s/Login/index", $_SERVER['HTTP_HOST'], __APP__);
    // 去掉最后一个字符，如果是反斜杠...
    $state = removeSlash($state);
    // 对链接地址进行base64加密...
    $state = urlsafe_b64encode($state);

    // 给模板设定数据 => default/Login/index.htm
    $this->assign('my_state', $state);
    $this->assign('my_scope', $this->m_weLogin['scope']);
    $this->assign('my_appid', $this->m_weLogin['appid']);
    $this->assign('my_redirect_uri', urlencode($this->m_weLogin['redirect_uri']));
    $this->display();
  }
  //
  // 根据cookie从数据库中获取当前登录用户信息 => 这里是接口调用，不能进行页面跳转...
  // 返回统一的结构：err_code | err_msg | data
  public function getWxUser()
  {
    // 设置默认的返回数组...
    $arrErr['err_code'] = false;
    $arrErr['err_msg'] = 'OK';
    // 判断用户是否已经处于登录状态 => 返回-1，让终端进行页面跳转...
    if( !Cookie::is_set('wx_unionid') || !Cookie::is_set('wx_headurl') || !Cookie::is_set('wx_ticker') ) {
      $arrErr['err_msg'] = 'cookie is not avail.';
      $arrErr['err_code'] = -1;
      return $arrErr;
    }
    // 通过cookie得到unionid，再用unionid查找用户信息...
    $strUnionid = Cookie::get('wx_unionid');
    // 准备服务器链接地址，去掉最后的反斜杠...
    $strServer = $this->m_weLogin['redirect_uri'];
    $strServer = removeSlash($strServer);
    // 准备请求链接地址，调用接口，返回数据...
    $strUrl = sprintf("%s/wxapi.php/Login/getWxUser/wx_unionid/%s", $strServer, $strUnionid);
    $result = http_get($strUrl);
    // 调用失败，返回错误信息...
    if( !$result ) {
      $arrErr['err_code'] = true;
      $arrErr['err_msg'] = sprintf("无法连接用户管理服务器。<br>%s", $strServer);
      return $arrErr;
    }
    // 将返回信息转换成数组...
    $arrResult = json_decode($result, true);
    // 如果没有需要的数据区，直接返回调用错误...
    if( !isset($arrResult['err_code']) || !isset($arrResult['data']) ) {
      $arrErr['err_code'] = true;
      $arrErr['err_msg'] = $result;
      return $arrErr;
    }
    // 返回最终的调用结果 => err_code | err_msg | data
    return $arrResult;
  }
  //
  // 显示错误模板信息...
  private function dispError($inMsgTitle, $inMsgDesc)
  {
    $this->assign('my_admin', true);
    $this->assign('my_title', '糟糕，出错了');
    $this->assign('my_msg_title', $inMsgTitle);
    $this->assign('my_msg_desc', $inMsgDesc);
    $this->display('Common:error_page');
  }
}
?>