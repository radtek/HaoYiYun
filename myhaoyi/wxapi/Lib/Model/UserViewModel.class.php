<?php

class UserViewModel extends ViewModel {
    public $viewFields = array(
        'User'=>array('*','_type'=>'LEFT'), //Must be LEFT 返回左表所有数据，即使右表没有匹配的
        'Node'=>array('node_id','node_tag','node_name','_on'=>'User.node_id=Node.node_id'),
    );
}
?>