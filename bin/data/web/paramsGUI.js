

// "preset": "Default",\
//   "closed": false,\
//   "remembered": {\
//     "Default": {\
//       "0": {\

//parameters with convenience functions for dat.gui to understand
//everything is JSON. subobjects render to subfolders named by their key
var paramsGUI = {

    shape : {
        display : 'selector',
        value: 'square',
        choices : ['square', 'circle'],
        onChange : function(val){
            valsChanged();
        }
    },
    
    x : {
        display : 'range',
        value : 1,
        min : 0,
        max : 5,
        onChange : function(val){
            valsChanged();
        },
        listen : true
    }

    
};