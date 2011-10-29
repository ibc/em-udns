module EventMachine::Udns

  class Query
    def callback &block
      @on_success_block = block
    end

    def errback &block
      @on_error_block = block
    end


    private

    def do_success result
      @on_success_block && @on_success_block.call(result)
    end

    def do_error error
      @on_error_block && @on_error_block.call(error)
    end
  end

end