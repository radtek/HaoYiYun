<?php

class GatherViewModel extends ViewModel {
    public $viewFields = array(
        'Gather'=>array('gather_id','user_id','node_id','ip_addr','mac_addr','name_pc','name_set','os_name','version','status','created','_type'=>'LEFT'),
        'Node'=>array('node_id','node_wan','node_type','node_proto','node_addr','_on'=>'Node.node_id=Gather.node_id'),
    );
}
?>