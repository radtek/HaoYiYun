<?php

class TrackViewModel extends ViewModel {
    public $viewFields = array(
        'Track'=>array('*','_type'=>'LEFT'), //Must be LEFT 返回左表所有数据，即使右表没有匹配的
        'User'=>array('user_id','wx_nickname','wx_headurl','_on'=>'User.user_id=Track.user_id','_type'=>'LEFT'),
        'Node'=>array('node_id','node_wan','node_type','node_proto','node_addr','_on'=>'Node.node_id=Track.node_id'),
    );
}
?>